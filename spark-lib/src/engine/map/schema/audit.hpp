#pragma once

#include "analysis.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace Engine::Map::Analysis {

    inline constexpr size_t auditProblemLimit = 0x1000;

    struct AuditProblem {
        ProblemKind kind;
        size_t startOffset;
        size_t endOffset;
    };

    struct AuditTagResult {
        uint32_t tagId;
        uint32_t groupId;
        std::string path;
        size_t problemCount = 0;
        std::vector<AuditProblem> problems;
        std::string error;
    };

    struct AuditGroupResult {
        size_t problemCount = 0;
        size_t storedProblemCount = 0;
        size_t errorCount = 0;
        std::vector<size_t> tags;
    };

    enum class AuditStatus { Running, Completed, Cancelled, Failed };

    struct AuditResults {
        AuditStatus status = AuditStatus::Running;
        size_t totalTags = 0;
        size_t completedTags = 0;
        size_t excludedTags = 0;
        size_t problemCount = 0;
        size_t storedProblemCount = 0;
        size_t errorCount = 0;
        std::string error;
        std::vector<AuditTagResult> tags;
        std::map<uint32_t, AuditGroupResult> groups;

        void addTag(AuditTagResult tag);
    };

    // The caller supplies a map snapshot; results hold offsets rather than pointers into it.
    class AuditJob {
    public:
        AuditJob(std::shared_ptr<MapFile> map, Schema schema,
            std::optional<uint32_t> group = {}, ProblemOptions options = {},
            bool excludeMissingSchema = false);
        ~AuditJob();
        AuditJob(const AuditJob&) = delete;
        AuditJob& operator=(const AuditJob&) = delete;

        void cancel();
        AuditResults snapshot() const;

    private:
        void run(std::shared_ptr<MapFile> map, Schema schema,
            std::optional<uint32_t> group, ProblemOptions options, bool excludeMissingSchema);

        std::atomic<bool> cancelled_ = false;
        mutable std::mutex mutex_;
        AuditResults results_;
        std::thread worker_;
    };

}
