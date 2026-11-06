#include "audit.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace Engine::Map::Analysis {

    void AuditResults::addTag(AuditTagResult tag) {
        size_t remaining = auditProblemLimit - storedProblemCount;
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

    void AuditJob::run(std::shared_ptr<MapFile> map, Schema schema,
        std::optional<uint32_t> group, ProblemOptions options, bool excludeMissingSchema) {
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
                        remaining = auditProblemLimit - results_.storedProblemCount;
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
                    results_.addTag(std::move(result));
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

}
