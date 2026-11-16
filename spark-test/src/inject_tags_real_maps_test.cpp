#include <catch_amalgamated.hpp>
#include <array>
#include <fstream>

#include "InjectionFiles.hpp"
#include "engine/map/bitmap.hpp"
#include "engine/map/model.hpp"
#include "engine/map/spark/static_tag_injection/configured_injection.hpp"
#include "engine/map/spark/static_tag_injection/inject_tags.hpp"

using namespace Engine;
using namespace Engine::Map;
using namespace Engine::Map::Spark;

namespace {
    std::filesystem::path backupMaps() {
        auto length = GetEnvironmentVariableW(L"MCC_HOME", nullptr, 0);
        REQUIRE(length > 0);
        std::wstring home(length, L'\0');
        auto written = GetEnvironmentVariableW(L"MCC_HOME", home.data(), length);
        REQUIRE(written > 0);
        REQUIRE(written < length);
        home.resize(written);
        auto directory = std::filesystem::path(home) / "halo1" / "maps_backup";
        REQUIRE(std::filesystem::is_directory(directory));
        return directory;
    }

    Schema injectionSchema() {
        auto path = std::filesystem::path(SPARK_TEST_DIR).parent_path() / "spark" / "shipfiles" / "tags.schema2.json";
        std::ifstream file(path);
        REQUIRE(file.is_open());
        return Schema::fromJsonString(std::string(std::istreambuf_iterator<char>(file), {}));
    }

    std::span<const uint8_t> checkedBytes(std::span<const uint8_t> bytes, size_t offset, size_t size) {
        REQUIRE(offset <= bytes.size());
        REQUIRE(size <= bytes.size() - offset);
        return bytes.subspan(offset, size);
    }

    template<class T> T read(std::span<const uint8_t> bytes, size_t offset) {
        auto data = checkedBytes(bytes, offset, sizeof(T));
        T value;
        std::memcpy(&value, data.data(), sizeof(value));
        return value;
    }

    uint32_t word(std::span<const uint8_t> bytes, size_t offset) {
        uint32_t value = read<uint32_t>(bytes, offset);
        return value;
    }
}

TEST_CASE("Prepare the installed tag manifest against an isolated b30 copy", "[.configured_real]") {
    auto installation = backupMaps().parent_path().parent_path();
    const auto installedTags = installation / "MCC" / "Binaries" / "Win64" / "tags";
    const auto manifest = installedTags / "mytags.json";
    const auto manifestBytes = Spool::readInjectionMap(manifest);
    const auto manifestText = std::string(manifestBytes.begin(), manifestBytes.end());
    const auto package = parseInjectionPackage(manifestText, manifest, "b30");
    REQUIRE_FALSE(package.tags.empty());
    struct Directory {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("spark-real-config-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        Directory() { REQUIRE(std::filesystem::create_directory(path)); }
        ~Directory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
            if (error) std::cerr << "Cannot clean real configured injection test: " << error.message() << '\n';
        }
    } temporary;
    const auto maps = temporary.path / "halo1" / "maps", tags = temporary.path / "tags";
    std::filesystem::create_directories(maps);
    std::filesystem::create_directory(tags);
    const auto host = installation / "halo1" / "maps" / "b30.map";
    const auto originalHost = Spool::readInjectionMap(host);
    const auto originalGuest = Spool::readInjectionMap(package.cache);
    REQUIRE(std::filesystem::copy_file(host, maps / "b30.map"));
    REQUIRE(std::filesystem::copy_file(manifest, tags / manifest.filename()));
    const auto relativeGuest = package.cache.lexically_relative(installedTags);
    std::filesystem::create_directories((tags / relativeGuest).parent_path());
    REQUIRE(std::filesystem::copy_file(package.cache, tags / relativeGuest));
    const auto schemaPath = std::filesystem::path(SPARK_TEST_DIR).parent_path() /
        "spark" / "shipfiles" / "tags.schema2.json";
    ConfiguredInjection service(maps, tags, schemaPath);
    auto prepared = service.prepare(maps / "b30.map", 1);
    REQUIRE(prepared.has_value());
    auto output = Spool::readInjectionMap(prepared->path);
    auto outputTags = listInjectionTags(output);
    for (const auto& selected : package.tags) {
        INFO(selected.path);
        REQUIRE(std::count_if(outputTags.begin(), outputTags.end(), [&](const InjectionTag& tag) {
            return tag.groupId == selected.groupId && tag.path == selected.path;
        }) == 1);
    }
    REQUIRE(service.prepare(maps / "b30.map", 1)->path == prepared->path);
    const auto handle = CreateFileW(prepared->path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(handle != INVALID_HANDLE_VALUE);
    std::array<uint8_t, 4> magic{};
    DWORD read = 0;
    const auto succeeded = ReadFile(handle, magic.data(), static_cast<DWORD>(magic.size()), &read, nullptr);
    REQUIRE(CloseHandle(handle));
    REQUIRE(succeeded);
    REQUIRE(read == magic.size());
    REQUIRE(std::memcmp(magic.data(), "daeh", magic.size()) == 0);
    REQUIRE(Spool::readInjectionMap(host) == originalHost);
    REQUIRE(Spool::readInjectionMap(package.cache) == originalGuest);
}

TEST_CASE("Backup maps expose valid scenario handles and guest tag lists", "[.real_maps]") {
    std::vector<std::filesystem::path> maps;
    for (const auto& entry : std::filesystem::directory_iterator(backupMaps()))
        if (entry.is_regular_file() && entry.path().extension() == ".map" && entry.path().filename() != "bitmaps.map")
            maps.push_back(entry.path());
    std::sort(maps.begin(), maps.end());
    REQUIRE_FALSE(maps.empty());
    for (const auto& path : maps) {
        INFO(path.string());
        auto bytes = Spool::readInjectionMap(path);
        const auto offset = word(bytes, 0x10);
        TagDataHeader header;
        REQUIRE(size_t(offset) + sizeof(header) <= bytes.size());
        std::memcpy(&header, bytes.data() + offset, sizeof(header));
        REQUIRE(header.scenarioId == word(bytes, size_t(offset) + 4));
        REQUIRE(header.checksum == word(bytes, size_t(offset) + 8));
        auto tags = listInjectionTags(bytes);
        REQUIRE(tags.size() == header.tagCount);
        REQUIRE_FALSE(tags.empty());
        REQUIRE(tags.front().groupId == GroupId_Scenario);
    }
}

TEST_CASE("Inject the d40 shotgun model into b30 using backup maps", "[.real_maps]") {
    const auto directory = backupMaps();
    auto host = Spool::readInjectionMap(directory / "b30.map");
    auto guest = Spool::readInjectionMap(directory / "d40.map");
    auto schema = injectionSchema();
    const std::string modelPath = R"(weapons\shotgun\shotgun)";
    auto guestTags = listInjectionTags(guest);
    auto hostTags = listInjectionTags(host);
    auto isModel = [&](const InjectionTag& tag) {
        return tag.groupId == GroupId_Model && tag.path == modelPath;
    };
    REQUIRE(std::count_if(guestTags.begin(), guestTags.end(), isModel) == 1);
    REQUIRE(std::none_of(hostTags.begin(), hostTags.end(), isModel));

    // The output exists only in memory. No existing map or output file is written.
    std::vector<uint8_t> output;
    try {
        output = injectTags(host, guest, {{GroupId_Model, modelPath.c_str()}}, schema);
    } catch (const std::runtime_error&) {
        REQUIRE(Spool::readInjectionMap(directory / "b30.map") == host);
        REQUIRE(Spool::readInjectionMap(directory / "d40.map") == guest);
        throw;
    }
    auto outputTags = listInjectionTags(output);
    REQUIRE(std::count_if(outputTags.begin(), outputTags.end(), isModel) == 1);
    REQUIRE(outputTags.size() > hostTags.size());
    REQUIRE(std::equal(hostTags.begin(), hostTags.end(), outputTags.begin(),
        [](const auto& a, const auto& b) { return a.groupId == b.groupId && a.path == b.path; }));

    auto metadata = [](std::span<const uint8_t> bytes, uint32_t pointer, size_t size) {
        const auto cache = read<CacheHeader>(bytes, 0);
        REQUIRE(pointer >= 0x50000000);
        const auto offset = pointer - 0x50000000;
        REQUIRE(offset <= cache.tagDataSize);
        REQUIRE(size <= cache.tagDataSize - offset);
        return checkedBytes(bytes, size_t(cache.tagData.offset) + offset, size);
    };
    auto tagAt = [&](const auto& bytes, size_t index) {
        const auto header = read<TagDataHeader>(bytes, word(bytes, 0x10));
        REQUIRE(index < header.tagCount);
        const auto table = metadata(bytes, header.tagArray.offset, size_t(header.tagCount) * sizeof(Tag));
        return read<Tag>(table, index * sizeof(Tag));
    };
    auto model = [&](const auto& bytes, const auto& tags) {
        const auto found = std::find_if(tags.begin(), tags.end(), isModel);
        REQUIRE(found != tags.end());
        const auto entry = tagAt(bytes, size_t(found - tags.begin()));
        return read<Model>(metadata(bytes, entry.data.offset, sizeof(Model)), 0);
    };
    const auto sourceModel = model(guest, guestTags), copiedModel = model(output, outputTags);
    const auto sourceHeader = read<TagDataHeader>(guest, word(guest, 0x10));
    const auto copiedHeader = read<TagDataHeader>(output, word(output, 0x10));
    REQUIRE(sourceHeader.vertexDataSize <= sourceHeader.modelDataSize);
    REQUIRE(copiedHeader.vertexDataSize <= copiedHeader.modelDataSize);
    const auto sourceVertices = checkedBytes(guest, sourceHeader.modelData.offset, sourceHeader.vertexDataSize);
    const auto copiedVertices = checkedBytes(output, copiedHeader.modelData.offset, copiedHeader.vertexDataSize);
    const auto sourceIndices = checkedBytes(guest,
        size_t(sourceHeader.modelData.offset) + sourceHeader.vertexDataSize,
        sourceHeader.modelDataSize - sourceHeader.vertexDataSize);
    const auto copiedIndices = checkedBytes(output,
        size_t(copiedHeader.modelData.offset) + copiedHeader.vertexDataSize,
        copiedHeader.modelDataSize - copiedHeader.vertexDataSize);
    REQUIRE(copiedModel.geometries.count == sourceModel.geometries.count);
    const auto sourceGeometries = metadata(guest, sourceModel.geometries.data.offset,
        size_t(sourceModel.geometries.count) * sizeof(Geometry));
    const auto copiedGeometries = metadata(output, copiedModel.geometries.data.offset,
        size_t(copiedModel.geometries.count) * sizeof(Geometry));
    size_t comparedParts = 0, vertexBytes = 0, indexBytes = 0;
    for (uint32_t i = 0; i < sourceModel.geometries.count; ++i) {
        INFO("shotgun geometry " << i);
        const auto sourceGeometry = read<Geometry>(sourceGeometries, size_t(i) * sizeof(Geometry));
        const auto copiedGeometry = read<Geometry>(copiedGeometries, size_t(i) * sizeof(Geometry));
        REQUIRE(copiedGeometry.parts.count == sourceGeometry.parts.count);
        if (!sourceGeometry.parts.count) continue;
        const auto sourceParts = metadata(guest, sourceGeometry.parts.data.offset,
            size_t(sourceGeometry.parts.count) * sizeof(Part));
        const auto copiedParts = metadata(output, copiedGeometry.parts.data.offset,
            size_t(copiedGeometry.parts.count) * sizeof(Part));
        for (uint32_t j = 0; j < sourceGeometry.parts.count; ++j) {
            INFO("shotgun part " << j);
            const auto sourcePart = read<Part>(sourceParts, size_t(j) * sizeof(Part));
            const auto copiedPart = read<Part>(copiedParts, size_t(j) * sizeof(Part));
            REQUIRE(sourcePart.vertexFormat < std::size(vertexSizes));
            REQUIRE(vertexSizes[sourcePart.vertexFormat] > 0);
            REQUIRE(copiedPart.vertexFormat == sourcePart.vertexFormat);
            REQUIRE(copiedPart.vertexCount == sourcePart.vertexCount);
            REQUIRE(copiedPart.indexCount == sourcePart.indexCount);
            REQUIRE(sourcePart.alsoIndices.offset == sourcePart.indices.offset);
            REQUIRE(copiedPart.alsoIndices.offset == copiedPart.indices.offset);
            const auto vertexSize = size_t(vertexSizes[sourcePart.vertexFormat]) * sourcePart.vertexCount;
            const auto indexSize = sizeof(uint16_t) * size_t(sourcePart.indexCount);
            const auto originalVertexData = checkedBytes(sourceVertices, sourcePart.union0.vertices.offset, vertexSize);
            const auto copiedVertexData = checkedBytes(copiedVertices, copiedPart.union0.vertices.offset, vertexSize);
            const auto originalIndexData = checkedBytes(sourceIndices, sourcePart.indices.offset, indexSize);
            const auto copiedIndexData = checkedBytes(copiedIndices, copiedPart.indices.offset, indexSize);
            const auto originalAlsoIndexData = checkedBytes(sourceIndices, sourcePart.alsoIndices.offset, indexSize);
            const auto copiedAlsoIndexData = checkedBytes(copiedIndices, copiedPart.alsoIndices.offset, indexSize);
            REQUIRE(std::memcmp(originalVertexData.data(), copiedVertexData.data(), vertexSize) == 0);
            REQUIRE(std::memcmp(originalIndexData.data(), copiedIndexData.data(), indexSize) == 0);
            REQUIRE(std::memcmp(originalAlsoIndexData.data(), copiedAlsoIndexData.data(), indexSize) == 0);
            ++comparedParts;
            vertexBytes += vertexSize;
            indexBytes += indexSize;
        }
    }
    REQUIRE(comparedParts > 0);
    REQUIRE(vertexBytes > 0);
    REQUIRE(indexBytes > 0);

    auto cachePixels = [](const std::filesystem::path& path, uint32_t offset, uint32_t size) {
        INFO("shared bitmap cache " << path.string());
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        REQUIRE(file.is_open());
        const auto length = file.tellg();
        REQUIRE(length >= 0);
        REQUIRE(uint64_t(offset) + size <= static_cast<uint64_t>(length));
        file.seekg(offset);
        REQUIRE(file.good());
        std::vector<uint8_t> pixels(size);
        file.read(reinterpret_cast<char*>(pixels.data()), size);
        REQUIRE(file.gcount() == size);
        REQUIRE(file.good());
        return pixels;
    };
    size_t comparedBitmaps = 0, comparedImages = 0, pixelBytes = 0;
    for (size_t i = hostTags.size(); i < outputTags.size(); ++i) {
        const auto& injected = outputTags[i];
        if (injected.groupId != GroupId_Bitmap) continue;
        INFO("injected bitmap " << injected.path);
        const auto found = std::find_if(guestTags.begin(), guestTags.end(), [&](const InjectionTag& tag) {
            return tag.groupId == injected.groupId && tag.path == injected.path;
        });
        REQUIRE(found != guestTags.end());
        const auto sourceTag = tagAt(guest, size_t(found - guestTags.begin()));
        const auto copiedTag = tagAt(output, i);
        const auto sourceBitmap = read<Bitmap>(metadata(guest, sourceTag.data.offset, sizeof(Bitmap)), 0);
        const auto copiedBitmap = read<Bitmap>(metadata(output, copiedTag.data.offset, sizeof(Bitmap)), 0);
        REQUIRE(copiedBitmap.bitmapData.count == sourceBitmap.bitmapData.count);
        ++comparedBitmaps;
        if (!sourceBitmap.bitmapData.count) continue;
        const auto sourceRecords = metadata(guest, sourceBitmap.bitmapData.data.offset,
            size_t(sourceBitmap.bitmapData.count) * sizeof(BitmapData));
        const auto copiedRecords = metadata(output, copiedBitmap.bitmapData.data.offset,
            size_t(copiedBitmap.bitmapData.count) * sizeof(BitmapData));
        for (uint32_t j = 0; j < sourceBitmap.bitmapData.count; ++j) {
            INFO("bitmap image " << j);
            const auto sourceData = read<BitmapData>(sourceRecords, size_t(j) * sizeof(BitmapData));
            const auto copiedData = read<BitmapData>(copiedRecords, size_t(j) * sizeof(BitmapData));
            auto expected = sourceData;
            expected.bitmapTagHandle = copiedTag.tagHandle;
            if (sourceData.dataSize && !(sourceData.flags & (1u << 8)))
                expected.data.offset = copiedData.data.offset;
            REQUIRE(std::memcmp(&expected, &copiedData, sizeof(BitmapData)) == 0);
            if (!sourceData.dataSize) continue;
            if (sourceData.flags & (1u << 8)) {
                const auto originalPixels = cachePixels(directory / "bitmaps.map",
                    sourceData.data.offset, sourceData.dataSize);
                const auto copiedPixels = cachePixels(directory.parent_path() / "maps" / "bitmaps.map",
                    copiedData.data.offset, copiedData.dataSize);
                REQUIRE(originalPixels == copiedPixels);
            } else {
                const auto originalPixels = checkedBytes(guest, sourceData.data.offset, sourceData.dataSize);
                const auto copiedPixels = checkedBytes(output, copiedData.data.offset, copiedData.dataSize);
                REQUIRE(std::memcmp(originalPixels.data(), copiedPixels.data(), sourceData.dataSize) == 0);
            }
            ++comparedImages;
            pixelBytes += sourceData.dataSize;
        }
    }
    REQUIRE(comparedBitmaps > 0);
    REQUIRE(comparedImages > 0);
    REQUIRE(pixelBytes > 0);
    REQUIRE(Spool::readInjectionMap(directory / "b30.map") == host);
    REQUIRE(Spool::readInjectionMap(directory / "d40.map") == guest);
}

TEST_CASE("Preserve real b30 cyborg animation DataPointer payloads", "[.real_maps][animation_data]") {
    const auto path = backupMaps() / "b30.map";
    const auto host = Spool::readInjectionMap(path);
    auto schema = injectionSchema();
    const auto tags = listInjectionTags(host);
    const auto found = std::find_if(tags.begin(), tags.end(), [](const InjectionTag& tag) {
        return tag.groupId == GroupId_Animation && tag.path == R"(characters\cyborg\cyborg)";
    });
    REQUIRE(found != tags.end());
    const auto index = static_cast<size_t>(found - tags.begin());
    auto* rootSchema = schema.structureForGroupId(found->groupId);
    REQUIRE(rootSchema != nullptr);
    const auto& animationsField = schema.fields.at(rootSchema->fields.at(0x74));
    REQUIRE(animationsField.type.type == Type::BlockPointer);
    const auto& animationSchema = schema.structures.at(animationsField.type.ref);
    REQUIRE(animationSchema.size == 180);

    std::vector<std::string> warnings;
    const auto output = injectTags(host, host, {}, schema, &warnings);
    auto meta = [](const auto& bytes, uint32_t pointer) {
        REQUIRE(pointer >= 0x50000000);
        const auto offset = size_t(word(bytes, 0x10)) + pointer - 0x50000000;
        REQUIRE(offset <= bytes.size());
        return offset;
    };
    auto root = [&](const auto& bytes) {
        const auto array = meta(bytes, word(bytes, word(bytes, 0x10)));
        return meta(bytes, word(bytes, array + index * sizeof(Tag) + 20));
    };
    const auto sourceRoot = root(host), outputRoot = root(output);
    const auto count = word(host, sourceRoot + animationsField.offset);
    REQUIRE(count > 0);
    REQUIRE(word(output, outputRoot + animationsField.offset) == count);
    const auto sourceAnimations = meta(host, word(host, sourceRoot + animationsField.offset + 4));
    const auto outputAnimations = meta(output, word(output, outputRoot + animationsField.offset + 4));
    size_t payloads = 0, paddedPayloads = 0;
    for (uint32_t i = 0; i < count; ++i) {
        for (const auto& [offset, id] : animationSchema.fields) {
            const auto& field = schema.fields.at(id);
            if (field.type.type != Type::DataPointer) continue;
            INFO("animation " << i << ", field " << field.name);
            REQUIRE_FALSE(field.external);
            const auto source = sourceAnimations + i * animationSchema.size + offset;
            const auto dest = outputAnimations + i * animationSchema.size + offset;
            const auto size = word(host, source);
            REQUIRE(word(output, dest) == size);
            for (const auto descriptorOffset : {4, 8, 16})
                REQUIRE(word(output, dest + descriptorOffset) == word(host, source + descriptorOffset));
            if (!size) {
                REQUIRE(word(output, dest + 12) == word(host, source + 12));
                continue;
            }
            const auto originalData = meta(host, word(host, source + 12));
            const auto copiedData = meta(output, word(output, dest + 12));
            REQUIRE(size <= host.size() - originalData);
            REQUIRE(size <= output.size() - copiedData);
            REQUIRE(std::memcmp(host.data() + originalData, output.data() + copiedData, size) == 0);
            ++payloads;
            if (word(host, source + 4)) ++paddedPayloads;
        }
    }
    REQUIRE(payloads == 656);
    REQUIRE(paddedPayloads == 174);
    REQUIRE(Spool::readInjectionMap(path) == host);
}

TEST_CASE("Preserve the real b30 external sky bitmap", "[.real_maps][external_bitmap]") {
    const auto path = backupMaps() / "b30.map";
    const auto original = Spool::readInjectionMap(path);
    auto host = original;
    auto tags = listInjectionTags(host);
    auto found = std::find_if(tags.begin(), tags.end(), [](const InjectionTag& tag) {
        return tag.groupId == GroupId_Bitmap && tag.path == R"(sky\planets\threshold clear afternoon)";
    });
    REQUIRE(found != tags.end());
    const auto tagData = word(host, 0x10);
    const auto array = size_t(tagData) + word(host, tagData) - 0x50000000;
    auto index = static_cast<size_t>(found - tags.begin());
    Tag selected;
    REQUIRE(array + (index + 1) * sizeof(Tag) <= host.size());
    std::memcpy(&selected, host.data() + array + index * sizeof(Tag), sizeof(selected));
    const auto root = size_t(tagData) + selected.data.offset - 0x50000000;
    const auto count = word(host, root + 0x60);
    REQUIRE(count > 0);
    const auto records = size_t(tagData) + word(host, root + 0x64) - 0x50000000;
    REQUIRE(records + size_t(count) * sizeof(BitmapData) <= host.size());
    for (uint32_t i = 0; i < count; ++i) {
        BitmapData bitmap;
        std::memcpy(&bitmap, host.data() + records + i * sizeof(bitmap), sizeof(bitmap));
        REQUIRE((bitmap.flags & (1u << 8)) != 0);
        REQUIRE(bitmap.data.offset > host.size());
    }

    // Isolate this real bitmap in an in-memory table; unrelated host schemas
    // must not hide regressions in external-cache preservation.
    selected.tagHandle &= 0xFFFF0000;
    std::memcpy(host.data() + array, &selected, sizeof(selected));
    const uint32_t noScenario = 0xFFFFFFFF, oneTag = 1;
    std::memcpy(host.data() + tagData + 4, &noScenario, sizeof(noScenario));
    std::memcpy(host.data() + tagData + 12, &oneTag, sizeof(oneTag));
    auto output = injectTags(host, host, {}, injectionSchema());
    auto outputTags = listInjectionTags(output);
    REQUIRE(outputTags.size() == 1);
    REQUIRE(outputTags[0].path == found->path);
    const auto outputData = word(output, 0x10);
    const auto outputArray = size_t(outputData) + word(output, outputData) - 0x50000000;
    const auto outputRoot = size_t(outputData) + word(output, outputArray + 20) - 0x50000000;
    REQUIRE(word(output, outputRoot + 0x60) == count);
    const auto outputRecords = size_t(outputData) + word(output, outputRoot + 0x64) - 0x50000000;
    REQUIRE(outputRecords + size_t(count) * sizeof(BitmapData) <= output.size());
    REQUIRE(std::memcmp(output.data() + outputRecords, original.data() + records,
        size_t(count) * sizeof(BitmapData)) == 0);
    REQUIRE(Spool::readInjectionMap(path) == original);
}
