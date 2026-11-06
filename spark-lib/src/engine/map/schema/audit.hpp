#pragma once

#include "analysis.hpp"
#include "../spark/managed_map_file.hpp"

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

    enum class AuditStatus { Pending, Running, Completed, Cancelled, Failed };

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

        void addTag(AuditTagResult tag, size_t limit = auditProblemLimit);
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

    struct AuditSource {
        std::filesystem::path path;
        // Already normalized; disk sources leave this empty.
        Spark::ManagedMapFilePtr snapshot;
    };

    struct AuditFileIdentity {
        size_t size = 0;
        uint64_t hash = 0;
        bool operator==(const AuditFileIdentity&) const = default;
    };

    struct AuditMapResult {
        std::filesystem::path path;
        bool fromSnapshot = false;
        std::optional<AuditFileIdentity> fileIdentity;
        AuditResults analysis;
    };

    struct AuditBatchResults {
        AuditStatus status = AuditStatus::Running;
        std::vector<AuditMapResult> maps;
        size_t completedMaps = 0;
        size_t failedMaps = 0;
        size_t completedTags = 0;
        size_t excludedTags = 0;
        size_t problemCount = 0;
        size_t storedProblemCount = 0;
        size_t errorCount = 0;
    };

    // One worker, one schema snapshot, and one detailed-problem budget for the batch.
    // Disk maps are released after scanning and reloaded only when following a link.
    class AuditBatchJob {
    public:
        AuditBatchJob(std::vector<AuditSource> sources, Schema schema,
            std::optional<uint32_t> group = {}, ProblemOptions options = {},
            bool excludeMissingSchema = false);
        ~AuditBatchJob();
        AuditBatchJob(const AuditBatchJob&) = delete;
        AuditBatchJob& operator=(const AuditBatchJob&) = delete;

        void cancel();
        AuditBatchResults snapshot() const;
        // Returns an independent inspection copy. Throws if the source changed or is unavailable.
        Spark::ManagedMapFilePtr openMap(size_t index) const;

    private:
        void run(std::optional<uint32_t> group, ProblemOptions options, bool excludeMissingSchema);

        std::vector<AuditSource> sources_;
        Schema schema_;
        std::atomic<bool> cancelled_ = false;
        mutable std::mutex mutex_;
        AuditBatchResults results_;
        std::thread worker_;
    };

}
