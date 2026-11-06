#include "audit.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace Engine::Map::Analysis {

    void AuditResults::addTag(AuditTagResult tag, size_t limit) {
        size_t remaining = limit > storedProblemCount ? limit - storedProblemCount : 0;
        if (tag.problems.size() > remaining) tag.problems.resize(remaining);
        auto& group = groups[tag.groupId];
        group.problemCount += tag.problemCount;
        group.storedProblemCount += tag.problems.size();
        group.errorCount += !tag.error.empty();
        group.tags.push_back(tags.size());
        problemCount += tag.problemCount;
        storedProblemCount += tag.problems.size();
        errorCount += !tag.error.empty();
        tags.push_back(std::move(tag));
        ++completedTags;
    }

    AuditJob::AuditJob(std::shared_ptr<MapFile> map, Schema schema,
        std::optional<uint32_t> group, ProblemOptions options, bool excludeMissingSchema) {
        if (!map) throw std::invalid_argument("Audit requires a map snapshot");
        worker_ = std::thread(&AuditJob::run, this, std::move(map), std::move(schema),
            group, options, excludeMissingSchema);
    }

    AuditJob::~AuditJob() {
        cancel();
        if (worker_.joinable()) worker_.join();
    }

    void AuditJob::cancel() { cancelled_ = true; }

    AuditResults AuditJob::snapshot() const {
        std::lock_guard lock(mutex_);
        return results_;
    }

    static void auditMap(std::shared_ptr<MapFile> map, Schema& schema,
        std::optional<uint32_t> group, ProblemOptions options, bool excludeMissingSchema,
        std::atomic<bool>& cancelled_, std::mutex& mutex_, AuditResults& results_, size_t limit) {
        try {
            auto* cache = map->getCacheHeader();
            const auto fileStart = reinterpret_cast<uintptr_t>(cache);
            const auto fileEnd = fileStart + cache->fileSize;
            std::vector<uint32_t> selected;
            std::vector<uintptr_t> starts;
            size_t excluded = 0;
            for (uint32_t i = 0; i < map->getTagCount() && !cancelled_; ++i) {
                auto* tag = map->getTag(i);
                if (!tag) throw std::runtime_error("Invalid tag table entry");
                auto address = reinterpret_cast<uintptr_t>(map->getTagData(tag));
                if (address >= fileStart && address < fileEnd) starts.push_back(address);
                if (group && tag->groupID != *group) continue;
                auto* root = schema.structureForGroupId(tag->groupID);
                if (excludeMissingSchema && (!root || root->fields.empty())) {
                    ++excluded;
                    continue;
                }
                selected.push_back(i);
            }
            std::sort(starts.begin(), starts.end());
            {
                std::lock_guard lock(mutex_);
                results_.totalTags = selected.size();
                results_.excludedTags = excluded;
            }
            Context ctx{map.get(), &schema};
            for (auto id : selected) {
                if (cancelled_) break;
                auto* tag = map->getTag(id);
                AuditTagResult result{tag->tagHandle, tag->groupID, ""};
                try {
                    auto path = reinterpret_cast<uintptr_t>(map->getTagPath(tag));
                    if (path < fileStart || path >= fileEnd)
                        throw std::runtime_error("Tag path lies outside the map");
                    auto* pathEnd = static_cast<const char*>(std::memchr(
                        reinterpret_cast<const void*>(path), 0, fileEnd - path));
                    if (!pathEnd) throw std::runtime_error("Unterminated tag path");
                    result.path.assign(reinterpret_cast<const char*>(path), pathEnd);
                    auto* root = schema.structureForGroupId(tag->groupID);
                    if (!root || root->fields.empty())
                        throw std::runtime_error("No mapped schema for this tag type");
                    auto start = reinterpret_cast<uintptr_t>(map->getTagData(tag));
                    if (start < fileStart || start >= fileEnd)
                        throw std::runtime_error("Tag data lies outside the map");
                    auto size = map->guessTagDataSize(tag->tagHandle);
                    // The last tag (or an out-of-order table) has no usable next-tag estimate.
                    if (!size || size > fileEnd - start) {
                        auto next = std::upper_bound(starts.begin(), starts.end(), start);
                        size = (next == starts.end() ? fileEnd : *next) - start;
                    }
                    StructureRef ref{&ctx, root, reinterpret_cast<void*>(start)};
                    auto analysis = analyzeStructure(ref, reinterpret_cast<void*>(start),
                        reinterpret_cast<void*>(start + size), options, (std::numeric_limits<size_t>::max)(), &cancelled_);
                    if (cancelled_) break;
                    result.problemCount = analysis.problems.size();
                    size_t remaining;
                    {
                        std::lock_guard lock(mutex_);
                        remaining = limit - results_.storedProblemCount;
                    }
                    for (const auto& problem : analysis.problems) {
                        if (result.problems.size() >= remaining) break;
                        result.problems.push_back({problem.kind,
                            reinterpret_cast<uintptr_t>(problem.start) - fileStart,
                            reinterpret_cast<uintptr_t>(problem.end) - fileStart});
                    }
                } catch (const std::exception& error) {
                    result.error = error.what();
                }
                if (cancelled_) break;
                {
                    std::lock_guard lock(mutex_);
                    results_.addTag(std::move(result), limit);
                }
            }
            std::lock_guard lock(mutex_);
            results_.status = cancelled_ ? AuditStatus::Cancelled : AuditStatus::Completed;
        } catch (const std::exception& error) {
            std::lock_guard lock(mutex_);
            results_.error = error.what();
            results_.status = AuditStatus::Failed;
        }
    }

    void AuditJob::run(std::shared_ptr<MapFile> map, Schema schema,
        std::optional<uint32_t> group, ProblemOptions options, bool excludeMissingSchema) {
        auditMap(std::move(map), schema, group, options, excludeMissingSchema,
            cancelled_, mutex_, results_, auditProblemLimit);
    }

    namespace {
        AuditFileIdentity fileIdentity(const Spark::ManagedMapFile& map) {
            // Hash the bytes before normalization, not the schema-dependent in-memory layout.
            uint64_t hash = 14695981039346656037ull;
            for (auto byte : map.bytes()) {
                hash ^= byte;
                hash *= 1099511628211ull;
            }
            return {map.bytes().size(), hash};
        }

        void validateAuditMap(Spark::ManagedMapFile& map) {
            auto* cache = map.getCacheHeader();
            auto size = map.bytes().size();
            auto range = [size](uint64_t offset, uint64_t length) {
                if (offset > size || length > size - offset)
                    throw std::runtime_error("Map section lies outside the file");
            };
            if (std::memcmp(cache->magic, "daeh", 4) || std::memcmp(cache->footerMagic, "toof", 4))
                throw std::runtime_error("Invalid map header");
            if (cache->fileSize != size || cache->tagData.offset < sizeof(CacheHeader))
                throw std::runtime_error("Invalid map size or tag section");
            range(cache->tagData.offset, cache->tagDataSize);
            if (cache->tagDataSize < sizeof(TagDataHeader))
                throw std::runtime_error("Truncated tag header");
            auto* header = map.getTagDataHeader();
            if (header->tagCount > 0xFFFF || header->tagArray.offset < 0x50000000)
                throw std::runtime_error("Invalid tag table");
            range(uint64_t(cache->tagData.offset) + header->tagArray.offset - 0x50000000,
                uint64_t(header->tagCount) * sizeof(Tag));
            bool hasBsp = false;
            for (uint32_t i = 0; i < header->tagCount; ++i) {
                auto* tag = map.getTag(i);
                if ((tag->tagHandle & 0xFFFF) != i)
                    throw std::runtime_error("Invalid tag handle");
                hasBsp |= tag->groupID == GroupId_BSP;
            }
            // BSP lookup accesses the first scenario's reference block before tag analysis.
            auto* first = map.getTag(0);
            if (hasBsp && first && first->groupID == GroupId_Scenario) {
                auto offset = uint64_t(cache->tagData.offset) + first->data.offset - 0x50000000;
                range(offset, sizeof(Scenario));
                auto* scenario = reinterpret_cast<const Scenario*>(map.bytes().data() + offset);
                if (scenario->structureBsps.count) {
                    auto refs = uint64_t(cache->tagData.offset) + scenario->structureBsps.data.offset - 0x50000000;
                    range(refs, uint64_t(scenario->structureBsps.count) * sizeof(StructureBspReference));
                    for (uint32_t i = 0; i < scenario->structureBsps.count; ++i) {
                        auto* ref = reinterpret_cast<const StructureBspReference*>(map.bytes().data() + refs) + i;
                        range(ref->structureBspHeader.offset, (std::max)(size_t(ref->size), sizeof(StructureBspHeader)));
                    }
                }
            }
        }
    }

    AuditBatchJob::AuditBatchJob(std::vector<AuditSource> sources, Schema schema,
        std::optional<uint32_t> group, ProblemOptions options, bool excludeMissingSchema)
        : sources_(std::move(sources)), schema_(std::move(schema)) {
        if (sources_.empty()) throw std::invalid_argument("Choose at least one map to audit");
        for (auto& source : sources_) {
            source.path = std::filesystem::absolute(source.path);
            AuditMapResult result;
            result.path = source.path;
            result.fromSnapshot = source.snapshot != nullptr;
            result.analysis.status = AuditStatus::Pending;
            results_.maps.push_back(std::move(result));
        }
        worker_ = std::thread(&AuditBatchJob::run, this, group, options, excludeMissingSchema);
    }

    AuditBatchJob::~AuditBatchJob() {
        cancel();
        if (worker_.joinable()) worker_.join();
    }

    void AuditBatchJob::cancel() { cancelled_ = true; }

    AuditBatchResults AuditBatchJob::snapshot() const {
        std::lock_guard lock(mutex_);
        auto result = results_;
        for (const auto& map : result.maps) {
            const auto& analysis = map.analysis;
            result.completedMaps += analysis.status == AuditStatus::Completed || analysis.status == AuditStatus::Failed;
            result.failedMaps += analysis.status == AuditStatus::Failed;
            result.completedTags += analysis.completedTags;
            result.excludedTags += analysis.excludedTags;
            result.problemCount += analysis.problemCount;
            result.storedProblemCount += analysis.storedProblemCount;
            result.errorCount += analysis.errorCount;
        }
        return result;
    }

    Spark::ManagedMapFilePtr AuditBatchJob::openMap(size_t index) const {
        std::optional<AuditFileIdentity> expected;
        {
            std::lock_guard lock(mutex_);
            expected = results_.maps.at(index).fileIdentity;
        }
        const auto& source = sources_.at(index);
        if (source.snapshot) return source.snapshot->snapshot();
        if (!expected) throw std::runtime_error("This map has not been loaded successfully by the audit");
        auto map = Spark::ManagedMapFile::create(source.path);
        if (!map) throw std::runtime_error("Cannot reopen audit map: " + source.path.string());
        if (fileIdentity(*map) != *expected)
            throw std::runtime_error("Map changed since it was audited. Rerun the audit before opening its results.");
        validateAuditMap(*map);
        auto schema = schema_;
        map->normalize(&schema);
        return map;
    }

    void AuditBatchJob::run(std::optional<uint32_t> group, ProblemOptions options, bool excludeMissingSchema) {
        size_t remaining = auditProblemLimit;
        auto schema = schema_;
        for (size_t i = 0; i < sources_.size(); ++i) {
            if (cancelled_) break;
            auto& result = results_.maps[i];
            {
                std::lock_guard lock(mutex_);
                result.analysis.status = AuditStatus::Running;
            }
            try {
                auto map = sources_[i].snapshot;
                if (!map) {
                    map = Spark::ManagedMapFile::create(sources_[i].path);
                    if (!map) throw std::runtime_error("Cannot read map: " + sources_[i].path.string());
                    validateAuditMap(*map);
                    auto identity = fileIdentity(*map);
                    if (cancelled_) break;
                    map->normalize(&schema);
                    std::lock_guard lock(mutex_);
                    result.fileIdentity = identity;
                }
                auditMap(map, schema, group, options, excludeMissingSchema,
                    cancelled_, mutex_, result.analysis, remaining);
                remaining -= result.analysis.storedProblemCount;
            } catch (const std::exception& error) {
                std::lock_guard lock(mutex_);
                result.analysis.error = error.what();
                result.analysis.status = AuditStatus::Failed;
            }
        }
        std::lock_guard lock(mutex_);
        for (auto& map : results_.maps)
            if (map.analysis.status == AuditStatus::Running || map.analysis.status == AuditStatus::Pending)
                map.analysis.status = AuditStatus::Cancelled;
        results_.status = cancelled_ ? AuditStatus::Cancelled : AuditStatus::Completed;
    }

}
