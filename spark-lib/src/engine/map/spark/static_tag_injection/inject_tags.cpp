#include "inject_tags.hpp"
#include "engine/map/bitmap.hpp"
#include "engine/map/model.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace Engine::Map::Spark {
    namespace {
        constexpr uint32_t tagBase = 0x50000000;
        constexpr uint32_t nullHandle = 0xFFFFFFFF;
        using Bytes = std::vector<uint8_t>;
        using Identity = std::pair<uint32_t, std::string>;
        static_assert(sizeof(CacheHeader) == 0x800 && sizeof(TagDataHeader) == 0x28);
        static_assert(sizeof(Tag) == 0x20 && sizeof(TagReference) == 0x10);
        static_assert(sizeof(StructureBspHeader) == 0x18 && sizeof(StructureBspReference) == 0x20);

        [[noreturn]] void fail(const std::string& message) {
            throw std::runtime_error("injectTags: " + message);
        }

        uint32_t narrow(size_t value) {
            if (value > std::numeric_limits<uint32_t>::max()) fail("32-bit size overflow");
            return static_cast<uint32_t>(value);
        }

        void range(size_t offset, size_t size, size_t limit) {
            if (offset > limit || size > limit - offset) fail("range outside its section");
        }

        template<class T> T read(std::span<const uint8_t> bytes, size_t offset) {
            range(offset, sizeof(T), bytes.size());
            T value;
            std::memcpy(&value, bytes.data() + offset, sizeof(T));
            return value;
        }

        template<class T> void write(Bytes& bytes, size_t offset, const T& value) {
            range(offset, sizeof(T), bytes.size());
            std::memcpy(bytes.data() + offset, &value, sizeof(T));
        }

        uint32_t append(Bytes& out, std::span<const uint8_t> bytes) {
            auto offset = narrow(out.size());
            narrow(out.size() + bytes.size());
            out.insert(out.end(), bytes.begin(), bytes.end());
            return offset;
        }

        void align(Bytes& bytes) {
            bytes.resize(narrow((bytes.size() + 3) & ~size_t(3)), 0);
        }

        struct Source {
            std::span<const uint8_t> bytes;
            CacheHeader cache;
            TagDataHeader header;
            std::vector<Tag> tags;
            std::map<Identity, uint32_t> identities;

            explicit Source(std::span<const uint8_t> input) : bytes(input) {
                cache = read<CacheHeader>(bytes, 0);
                if (std::memcmp(cache.magic, "daeh", 4) ||
                    std::memcmp(cache.footerMagic, "toof", 4)) fail("invalid cache magic");
                if (cache.cacheVersion != 13) fail("only MCC CEA cache version 13 is supported");
                if (cache.paddingLength) fail("nonzero cache paddingLength is unsupported");
                if (cache.fileSize != bytes.size()) fail("fileSize does not match input span");
                if (cache.tagData.offset < sizeof(CacheHeader)) fail("tag section overlaps header");
                range(cache.tagData.offset, cache.tagDataSize, bytes.size());
                if (size_t(cache.tagData.offset) + cache.tagDataSize != bytes.size())
                    fail("tag data must be the final section");
                if (cache.tagDataSize < sizeof(TagDataHeader)) fail("truncated tag header");
                header = read<TagDataHeader>(bytes, cache.tagData.offset);
                if (std::memcmp(header.magic, "sgat", 4)) fail("invalid tag-data magic");
                if (header.tagCount > 0xFFFF) fail("tag count exceeds handle index capacity");
                if (header.vertexDataSize > header.modelDataSize) fail("invalid model sizes");
                if (header.modelData.offset < sizeof(CacheHeader)) fail("invalid fixed prefix");
                range(header.modelData.offset, header.modelDataSize, cache.tagData.offset);
                auto modelEnd = size_t(header.modelData.offset) + header.modelDataSize;
                if (cache.tagData.offset - modelEnd > 3) fail("unexpected gap after model data");
                for (auto at = modelEnd; at < cache.tagData.offset; ++at)
                    if (bytes[at]) fail("nonzero model alignment padding");
                const auto array = meta(header.tagArray.offset, size_t(header.tagCount) * sizeof(Tag));
                for (uint32_t i = 0; i < header.tagCount; ++i) {
                    auto tag = read<Tag>(bytes, array + size_t(i) * sizeof(Tag));
                    if ((tag.tagHandle & 0xFFFF) != i || tag.tagHandle == nullHandle)
                        fail("invalid tag handle/index");
                    auto path = name(tag.path.offset);
                    if (!identities.emplace(Identity{tag.groupID, path}, i).second)
                        fail("duplicate tag identity");
                    tags.push_back(tag);
                }
                if (header.scenarioId != nullHandle) index(header.scenarioId);
            }

            uint32_t meta(uint32_t pointer, size_t size) const {
                if (pointer < tagBase) fail("invalid metadata pointer");
                auto offset = pointer - tagBase;
                range(offset, size, cache.tagDataSize);
                return narrow(size_t(cache.tagData.offset) + offset);
            }

            std::string name(uint32_t pointer) const {
                auto offset = meta(pointer, 1);
                auto begin = reinterpret_cast<const char*>(bytes.data() + offset);
                auto end = static_cast<const char*>(std::memchr(begin, 0, bytes.size() - offset));
                if (!end) fail("unterminated tag name");
                return std::string(begin, end);
            }

            uint32_t index(uint32_t handle) const {
                auto i = handle & 0xFFFF;
                if (i >= tags.size() || tags[i].tagHandle != handle) fail("stale or missing tag handle");
                return i;
            }

            void fixed(uint32_t offset, size_t size) const {
                if (offset < sizeof(CacheHeader)) fail("resource overlaps cache header");
                range(offset, size, header.modelData.offset);
            }
        };

        enum class Section { Fixed, Host, Guest, Names, Bitmaps, Literal };

        struct Patch {
            Section target;
            uint32_t value;
            bool operator==(const Patch&) const = default;
        };

        struct AddressSpace {
            bool bsp = false;
            uint32_t file = 0;
            uint32_t base = 0;
            uint32_t size = 0;
        };

        struct Injector {
            Source host;
            Source guest;
            const Schema& schema;
            Bytes names;
            Bytes bitmaps;
            std::map<std::string, uint32_t> nameOffsets;
            std::map<std::pair<Section, uint32_t>, Patch> patches;
            std::map<uint32_t, uint32_t> guestHandles;
            std::vector<uint32_t> imports;
            std::set<std::tuple<bool, uint32_t, Id, uint32_t>> visited;
            std::map<Id, size_t> structureExtents;
            std::vector<uint32_t> tableNames;
            std::vector<uint32_t> tableData;
            uint32_t importedParts = 0;
            bool copyGuestModels = false;

            Injector(std::span<const uint8_t> h, std::span<const uint8_t> g, const Schema& s)
                : host(h), guest(g), schema(s) {
                if (host.cache.cacheVersion != guest.cache.cacheVersion) fail("cache versions differ");
            }

            uint32_t intern(const std::string& name) {
                auto found = nameOffsets.find(name);
                if (found != nameOffsets.end()) return found->second;
                auto offset = narrow(names.size());
                append(names, {reinterpret_cast<const uint8_t*>(name.c_str()), name.size() + 1});
                nameOffsets.emplace(name, offset);
                return offset;
            }

            const StructureNode& structure(const Id& id) const {
                auto found = schema.structures.find(id);
                if (found == schema.structures.end() || found->second.size == 0)
                    fail("missing or empty structure schema: " + id);
                if (id.empty() || found->second.id != id) fail("inconsistent structure schema ID");
                return found->second;
            }

            const StructureNode& root(uint32_t group) const {
                const StructureNode* result = nullptr;
                for (const auto& [id, node] : schema.structures) {
                    if (static_cast<uint32_t>(node.groupId) != group) continue;
                    if (id.empty() || node.id != id) fail("inconsistent group schema ID");
                    if (result) fail("ambiguous group schema");
                    result = &node;
                }
                if (!result || result->fields.empty()) fail("missing or empty tag-group schema: " + std::to_string(group));
                return *result;
            }

            size_t checkSchema(const StructureNode& node) {
                auto checked = structureExtents.find(node.id);
                if (checked != structureExtents.end()) return checked->second;
                size_t cursor = 0;
                for (const auto& [offset, id] : node.fields) {
                    auto found = schema.fields.find(id);
                    if (found == schema.fields.end()) fail("missing field schema: " + id);
                    const auto& field = found->second;
                    auto type = static_cast<size_t>(field.type.type);
                    if (type >= TypeCount) fail("invalid schema field type");
                    if (field.offset != offset || offset < cursor)
                        fail("schema offset mismatch or overlap in " + node.id);
                    size_t size = field.type.type == Type::Structure
                        ? structure(field.type.ref).size : TypeSizes[type];
                    range(offset, size, node.size ? node.size : std::numeric_limits<uint32_t>::max());
                    cursor = offset + size;
                }
                // Root sizes may be unspecified. Only the mapped field extent is
                // needed: the complete metadata backing is copied independently.
                auto extent = node.size ? node.size : cursor;
                structureExtents.emplace(node.id, extent);
                return extent;
            }

            void patch(bool fromGuest, uint32_t file, AddressSpace space, Patch value) {
                auto section = space.bsp ? Section::Fixed : fromGuest ? Section::Guest : Section::Host;
                const auto& source = fromGuest ? guest : host;
                auto offset = space.bsp ? file : file - source.cache.tagData.offset;
                auto next = patches.lower_bound({section, offset});
                if (next != patches.end() && next->first.first == section &&
                    next->first.second != offset && size_t(offset) + 4 > next->first.second)
                    fail("overlapping fixup locations");
                if (next != patches.begin()) {
                    auto previous = std::prev(next);
                    if (previous->first.first == section && size_t(previous->first.second) + 4 > offset)
                        fail("overlapping fixup locations");
                }
                auto [it, inserted] = patches.emplace(std::make_pair(section, offset), value);
                if (!inserted && !(it->second == value)) fail("conflicting aliased fixups");
            }

            uint32_t resolve(uint32_t index) {
                auto existing = guestHandles.find(index);
                if (existing != guestHandles.end()) return existing->second;
                const auto& tag = guest.tags.at(index);
                Identity identity{tag.groupID, guest.name(tag.path.offset)};
                auto found = host.identities.find(identity);
                uint32_t handle;
                if (found != host.identities.end()) {
                    handle = host.tags[found->second].tagHandle;
                } else {
                    if (tag.groupID == GroupId_BSP || tag.groupID == GroupId_Scenario)
                        fail("importing scenarios/BSPs is unsupported");
                    auto newIndex = host.tags.size() + imports.size();
                    if (newIndex >= 0xFFFF) fail("tag index capacity exhausted");
                    handle = (tag.tagHandle & 0xFFFF0000) | narrow(newIndex);
                    imports.push_back(index);
                }
                guestHandles.emplace(index, handle);
                return handle;
            }

            uint32_t address(const Source& source, uint32_t pointer, size_t size, AddressSpace space) {
                if (!space.bsp) return source.meta(pointer, size);
                if (pointer < space.base) fail("invalid BSP-local pointer");
                range(pointer - space.base, size, space.size);
                return narrow(size_t(space.file) + pointer - space.base);
            }

            void pointer(bool fromGuest, uint32_t file, size_t size, AddressSpace space) {
                const auto& source = fromGuest ? guest : host;
                auto value = read<uint32_t>(source.bytes, file);
                address(source, value, size, space);
                if (!space.bsp)
                    patch(fromGuest, file, space, {fromGuest ? Section::Guest : Section::Host, value - tagBase});
            }

            void walk(bool fromGuest, uint32_t file, const StructureNode& node, AddressSpace space, size_t depth = 0) {
                if (depth > 256) fail("metadata nesting exceeds 256");
                const auto& source = fromGuest ? guest : host;
                const size_t start = space.bsp ? space.file : source.cache.tagData.offset;
                const size_t size = space.bsp ? space.size : source.cache.tagDataSize;
                if (file < start) fail("structure outside metadata");
                range(file - start, checkSchema(node), size);
                if (!visited.emplace(fromGuest, file, node.id, space.base).second) return;
                for (const auto& [offset, id] : node.fields) {
                    const auto& field = schema.fields.at(id);
                    auto at = narrow(size_t(file) + offset);
                    switch (field.type.type) {
                    case Type::Structure:
                        walk(fromGuest, at, structure(field.type.ref), space, depth + 1);
                        break;
                    case Type::BlockPointer: {
                        auto count = read<uint32_t>(source.bytes, at);
                        if (!count) break;
                        const auto& child = structure(field.type.ref);
                        if (child.size > size || count > size / child.size) fail("block extent overflow");
                        auto value = read<uint32_t>(source.bytes, at + 4);
                        auto block = address(source, value, size_t(count) * child.size, space);
                        pointer(fromGuest, at + 4, size_t(count) * child.size, space);
                        for (uint32_t i = 0; i < count; ++i)
                            walk(fromGuest, narrow(block + size_t(i) * child.size), child, space, depth + 1);
                        break;
                    }
                    case Type::DataPointer: {
                        auto length = read<int32_t>(source.bytes, at);
                        if (length < 0) fail("negative DataPointer size");
                        if (!length) break;
                        if (read<uint32_t>(source.bytes, at + 4) || read<uint32_t>(source.bytes, at + 8))
                            fail("external/flagged DataPointer payload is unsupported");
                        pointer(fromGuest, at + 12, static_cast<size_t>(length), space);
                        break;
                    }
                    case Type::TagReference: {
                        auto ref = read<TagReference>(source.bytes, at);
                        if (ref.path.offset)
                            patch(fromGuest, at + 4, space, {Section::Names, intern(source.name(ref.path.offset))});
                        if (ref.tagHandle == nullHandle) break;
                        auto index = source.index(ref.tagHandle);
                        const auto& target = source.tags[index];
                        if (ref.groupID != target.groupID && ref.groupID != target.parentGroupID &&
                            ref.groupID != target.grandparentGroupID) fail("tag-reference group mismatch");
                        if (fromGuest) {
                            auto handle = resolve(index);
                            patch(true, at + 12, space, {Section::Literal, handle});
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }
            }

            AddressSpace bspSpace(const Tag& tag) {
                if (tag.data.offset) fail("nonzero host BSP tag-data pointer is unsupported");
                if (host.header.scenarioId == nullHandle) fail("BSP has no scenario");
                const auto& scenarioTag = host.tags[host.index(host.header.scenarioId)];
                if (scenarioTag.groupID != GroupId_Scenario) fail("invalid scenario group");
                auto scenario = read<Scenario>(host.bytes, host.meta(scenarioTag.data.offset, sizeof(Scenario)));
                auto count = scenario.structureBsps.count;
                auto block = host.meta(scenario.structureBsps.data.offset, size_t(count) * sizeof(StructureBspReference));
                for (uint32_t i = 0; i < count; ++i) {
                    auto ref = read<StructureBspReference>(host.bytes, block + size_t(i) * sizeof(StructureBspReference));
                    if (ref.tagReference.tagHandle != tag.tagHandle) continue;
                    host.fixed(ref.structureBspHeader.offset, ref.size);
                    if (ref.size < sizeof(StructureBspHeader)) fail("truncated BSP header");
                    auto header = read<StructureBspHeader>(host.bytes, ref.structureBspHeader.offset);
                    if (std::memcmp(header.magic, "psbs", 4)) fail("invalid BSP magic");
                    if (header.vertexBufferSize < 0) fail("negative BSP vertex-buffer size");
                    if (header.vertexBufferSize)
                        host.fixed(header.vertexBuffer.offset, static_cast<size_t>(header.vertexBufferSize));
                    return {true, ref.structureBspHeader.offset, ref.runtimeAddress.offset, ref.size};
                }
                fail("missing scenario BSP reference");
            }

            void bitmap(bool fromGuest, uint32_t file, uint32_t handle) {
                const auto& source = fromGuest ? guest : host;
                source.meta(tagBase + file - source.cache.tagData.offset, sizeof(Bitmap));
                auto bitmap = read<Bitmap>(source.bytes, file);
                if (!bitmap.bitmapData.count) return;
                auto block = source.meta(bitmap.bitmapData.data.offset, size_t(bitmap.bitmapData.count) * sizeof(BitmapData));
                for (uint32_t i = 0; i < bitmap.bitmapData.count; ++i) {
                    auto at = narrow(block + size_t(i) * sizeof(BitmapData));
                    auto data = read<BitmapData>(source.bytes, at);
                    if (data.flags & (1u << 8)) fail("external bitmap cache is unsupported");
                    if (fromGuest)
                        patch(true, at + offsetof(BitmapData, bitmapTagHandle), {}, {Section::Literal, handle});
                    if (!data.dataSize) continue;
                    source.fixed(data.data.offset, data.dataSize);
                    if (fromGuest) {
                        align(bitmaps);
                        auto offset = append(bitmaps, source.bytes.subspan(data.data.offset, data.dataSize));
                        patch(true, at + offsetof(BitmapData, data), {}, {Section::Bitmaps, offset});
                    }
                }
            }

            void model(bool fromGuest, uint32_t file) {
                const auto& source = fromGuest ? guest : host;
                source.meta(tagBase + file - source.cache.tagData.offset, sizeof(Model));
                auto model = read<Model>(source.bytes, file);
                if (fromGuest) copyGuestModels = true;
                if (!model.geometries.count) return;
                auto geometries = source.meta(model.geometries.data.offset, size_t(model.geometries.count) * sizeof(Geometry));
                for (uint32_t i = 0; i < model.geometries.count; ++i) {
                    auto geometry = read<Geometry>(source.bytes, geometries + size_t(i) * sizeof(Geometry));
                    if (!geometry.parts.count) continue;
                    auto parts = source.meta(geometry.parts.data.offset, size_t(geometry.parts.count) * sizeof(Part));
                    if (fromGuest) importedParts = narrow(size_t(importedParts) + geometry.parts.count);
                    for (uint32_t j = 0; j < geometry.parts.count; ++j) {
                        auto at = narrow(parts + size_t(j) * sizeof(Part));
                        auto part = read<Part>(source.bytes, at);
                        if (part.runtimeVertices.offset) fail("runtime-initialized model vertices are unsupported");
                        if (part.vertexFormat >= std::size(vertexSizes) || !vertexSizes[part.vertexFormat])
                            fail("unsupported model vertex format");
                        range(part.union0.vertices.offset, size_t(vertexSizes[part.vertexFormat]) * part.vertexCount,
                            source.header.vertexDataSize);
                        range(part.indices.offset, size_t(part.indexCount) * sizeof(uint16_t),
                            source.header.modelDataSize - source.header.vertexDataSize);
                        if (fromGuest) {
                            patch(true, at + offsetof(Part, union0), {}, {Section::Literal,
                                narrow(size_t(part.union0.vertices.offset) + host.header.vertexDataSize)});
                            patch(true, at + offsetof(Part, indices), {}, {Section::Literal,
                                narrow(size_t(part.indices.offset) + host.header.modelDataSize - host.header.vertexDataSize)});
                        }
                    }
                }
            }

            void tag(bool fromGuest, uint32_t index) {
                const auto& source = fromGuest ? guest : host;
                const auto& entry = source.tags[index];
                tableNames.push_back(intern(source.name(entry.path.offset)));
                if (entry.groupID == GroupId_BSP) {
                    auto space = bspSpace(entry);
                    auto header = read<StructureBspHeader>(host.bytes, space.file);
                    auto& node = root(entry.groupID);
                    walk(false, address(host, header.structureBsp.offset, checkSchema(node), space), node, space);
                    tableData.push_back(0);
                } else {
                    const auto& node = root(entry.groupID);
                    auto file = source.meta(entry.data.offset, checkSchema(node));
                    tableData.push_back(entry.data.offset - tagBase);
                    walk(fromGuest, file, node, {});
                    if (entry.groupID == GroupId_Bitmap)
                        bitmap(fromGuest, file, fromGuest ? guestHandles.at(index) : entry.tagHandle);
                    if (entry.groupID == GroupId_Model) model(fromGuest, file);
                }
            }

            Bytes run(const TagList& requested) {
                for (const auto& id : requested) {
                    if (!id.path) fail("null requested tag path");
                    auto found = guest.identities.find({id.groupId, id.path});
                    if (found == guest.identities.end()) fail("requested guest tag not found: " + std::string(id.path));
                    resolve(found->second);
                }
                for (uint32_t i = 0; i < host.tags.size(); ++i) tag(false, i);
                for (size_t i = 0; i < imports.size(); ++i) tag(true, imports[i]);

                Bytes out;
                append(out, host.bytes.first(host.header.modelData.offset));
                const auto bitmapOffset = append(out, bitmaps);
                align(out);
                const auto modelOffset = narrow(out.size());
                append(out, host.bytes.subspan(host.header.modelData.offset, host.header.vertexDataSize));
                if (copyGuestModels)
                    append(out, guest.bytes.subspan(guest.header.modelData.offset, guest.header.vertexDataSize));
                const auto vertexSize = narrow(out.size() - modelOffset);
                append(out, host.bytes.subspan(size_t(host.header.modelData.offset) + host.header.vertexDataSize,
                    host.header.modelDataSize - host.header.vertexDataSize));
                if (copyGuestModels)
                    append(out, guest.bytes.subspan(size_t(guest.header.modelData.offset) + guest.header.vertexDataSize,
                        guest.header.modelDataSize - guest.header.vertexDataSize));
                const auto modelSize = narrow(out.size() - modelOffset);
                align(out);
                const auto tagOffset = narrow(out.size());
                const auto count = host.tags.size() + imports.size();
                out.resize(narrow(out.size() + sizeof(TagDataHeader) + count * sizeof(Tag)), 0);
                const auto namesOffset = append(out, names);
                align(out);
                const auto hostOffset = append(out, host.bytes.subspan(host.cache.tagData.offset, host.cache.tagDataSize));
                align(out);
                const auto guestOffset = narrow(out.size());
                if (!imports.empty())
                    append(out, guest.bytes.subspan(guest.cache.tagData.offset, guest.cache.tagDataSize));

                auto encoded = [&](size_t offset) { return narrow(size_t(tagBase) + offset - tagOffset); };
                auto sectionOffset = [&](Section section) -> uint32_t {
                    switch (section) {
                    case Section::Fixed: return 0;
                    case Section::Host: return hostOffset;
                    case Section::Guest: return guestOffset;
                    case Section::Names: return namesOffset;
                    case Section::Bitmaps: return bitmapOffset;
                    default: fail("invalid patch section");
                    }
                };
                for (const auto& [location, patch] : patches) {
                    auto value = patch.value;
                    if (patch.target != Section::Literal) {
                        auto target = size_t(sectionOffset(patch.target)) + patch.value;
                        value = patch.target == Section::Bitmaps ? narrow(target) : encoded(target);
                    }
                    write(out, size_t(sectionOffset(location.first)) + location.second, value);
                }
                for (size_t i = 0; i < count; ++i) {
                    bool fromGuest = i >= host.tags.size();
                    auto entry = fromGuest ? guest.tags[imports[i - host.tags.size()]] : host.tags[i];
                    if (fromGuest) entry.tagHandle = guestHandles.at(imports[i - host.tags.size()]);
                    entry.path.offset = encoded(size_t(namesOffset) + tableNames[i]);
                    if (entry.groupID != GroupId_BSP)
                        entry.data.offset = encoded(size_t(fromGuest ? guestOffset : hostOffset) + tableData[i]);
                    write(out, size_t(tagOffset) + sizeof(TagDataHeader) + i * sizeof(Tag), entry);
                }
                auto header = host.header;
                header.tagArray.offset = tagBase + sizeof(TagDataHeader);
                header.tagCount = narrow(count);
                header.modelData.offset = modelOffset;
                header.vertexDataSize = vertexSize;
                header.modelDataSize = modelSize;
                header.modelPartCount = narrow(size_t(header.modelPartCount) + importedParts);
                header.modelPartCount2 = narrow(size_t(header.modelPartCount2) + importedParts);
                write(out, tagOffset, header);
                auto cache = host.cache;
                cache.tagData.offset = tagOffset;
                cache.tagDataSize = narrow(out.size() - tagOffset);
                cache.fileSize = narrow(out.size());
                cache.paddingLength = 0;
                write(out, 0, cache);
                Source validated(out);
                return out;
            }
        };
    }

    std::vector<uint8_t> injectTags(
        std::span<const uint8_t> hostBytes,
        std::span<const uint8_t> guestBytes,
        const TagList& guestTags,
        const Schema& schema
    ) {
        return Injector(hostBytes, guestBytes, schema).run(guestTags);
    }
}
