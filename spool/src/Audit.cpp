#include "App.hpp"
#include "FileDialog.hpp"

#include "engine/map/schema/schema_file.hpp"
#include "ui/dissect/DissectTag.hpp"
#include "utils/Strings.hpp"

#include <set>
#include <stdexcept>
#include <algorithm>

namespace Spool {

    void App::startAudit() {
        try {
            auto* schema = Engine::Map::getMainSchema();
            if (!schema) throw std::runtime_error("No tag schema is loaded");
            std::vector<Engine::Map::Analysis::AuditSource> sources;
            if (auditDiskMaps_) {
                for (const auto& path : auditPaths_) sources.push_back({path, {}});
            } else {
                if (!map_) throw std::runtime_error("Open a map or select maps from disk first");
                sources.push_back({map_->getFilePath(), map_->snapshot()});
            }
            audit_ = std::make_unique<Engine::Map::Analysis::AuditBatchJob>(
                std::move(sources), *schema, auditGroup_, auditOptions_, auditExcludeMissingSchema_,
                auditExcludeBsp_ ? std::optional<uint32_t>(Engine::GroupId_BSP) : std::nullopt);
            auditViews_.clear();
            auditError_.clear();
        } catch (const std::exception& error) {
            auditError_ = std::string("Could not start audit: ") + error.what();
            MessageBoxA(window_, auditError_.c_str(), "Spool - Audit", MB_OK | MB_ICONERROR);
        }
    }

    void App::openAuditTag(size_t mapIndex, uint32_t tagId) {
        try {
            auto map = auditViews_[mapIndex].lock();
            if (!map) {
                map = audit_->openMap(mapIndex);
                auditViews_[mapIndex] = map;
            }
            auto* tag = map->getTag(tagId);
            if (!tag || tag->tagHandle != tagId)
                throw std::runtime_error("The audited tag is not present in this map. Rerun the audit.");
            SparkLib::UI::DissectTag::openWindow(map, tagId, map->getFilePath().filename().string() + " (audit)");
            auditError_.clear();
        } catch (const std::exception& error) {
            auditError_ = std::string("Could not open audit result: ") + error.what();
            MessageBoxA(window_, auditError_.c_str(), "Spool - Audit", MB_OK | MB_ICONERROR);
        }
    }

    void App::auditWindow() {
        using namespace Engine::Map::Analysis;
        if (!showAudit_) return;
        ImGui::SetNextWindowSize(ImVec2(850, 620), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Audit", &showAudit_)) {
            ImGui::End();
            return;
        }

        AuditBatchResults results;
        if (audit_) results = audit_->snapshot();
        bool running = audit_ && results.status == AuditStatus::Running;
        char progress[128];
        snprintf(progress, sizeof(progress), "%zu / %zu maps; %zu tags%s",
            results.completedMaps, results.maps.size(), results.completedTags, running ? " - analyzing" : "");
        float fraction = results.maps.empty() ? 0.0f
            : static_cast<float>(results.completedMaps) / static_cast<float>(results.maps.size());
        ImGui::ProgressBar(fraction, ImVec2(-1, 0), audit_ ? progress : "Not started");
        for (const auto& map : results.maps) {
            if (map.analysis.status != AuditStatus::Running) continue;
            auto name = map.path.filename().string();
            snprintf(progress, sizeof(progress), "%zu / %zu tags", map.analysis.completedTags, map.analysis.totalTags);
            ImGui::TextUnformatted(name.c_str());
            ImGui::ProgressBar(map.analysis.totalTags
                ? float(map.analysis.completedTags) / float(map.analysis.totalTags) : 0.0f,
                ImVec2(-1, 0), map.analysis.totalTags ? progress : "Loading / selecting tags");
        }

        ImGui::BeginDisabled(running);
        if (ImGui::RadioButton("Current map snapshot", !auditDiskMaps_)) auditDiskMaps_ = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Maps from disk", auditDiskMaps_)) auditDiskMaps_ = true;
        if (auditDiskMaps_) {
            if (ImGui::Button("Add Maps...")) {
                try {
                    for (auto path : pickMapFiles(window_)) {
                        path = std::filesystem::weakly_canonical(path);
                        bool duplicate = std::any_of(auditPaths_.begin(), auditPaths_.end(), [&](const auto& existing) {
                            return _wcsicmp(existing.c_str(), path.c_str()) == 0;
                        });
                        if (!duplicate) auditPaths_.push_back(std::move(path));
                    }
                } catch (const std::exception& error) {
                    auditError_ = std::string("Could not select maps: ") + error.what();
                    MessageBoxA(window_, auditError_.c_str(), "Spool - Audit", MB_OK | MB_ICONERROR);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Selection")) auditPaths_.clear();
            ImGui::SameLine();
            ImGui::Text("%zu maps selected", auditPaths_.size());
            if (!auditPaths_.empty()) {
                ImGui::BeginChild("Selected maps", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5),
                    ImGuiChildFlags_Borders);
                for (size_t i = 0; i < auditPaths_.size();) {
                    ImGui::PushID(static_cast<int>(i));
                    bool remove = ImGui::SmallButton("Remove");
                    ImGui::SameLine();
                    ImGui::TextUnformatted(auditPaths_[i].string().c_str());
                    ImGui::PopID();
                    if (remove) auditPaths_.erase(auditPaths_.begin() + i);
                    else ++i;
                }
                ImGui::EndChild();
            }
        } else if (map_) {
            ImGui::TextWrapped("Map: %s", map_->getFilePath().string().c_str());
        } else {
            ImGui::TextUnformatted("Open a map or choose Maps from disk.");
            if (ImGui::Button("Open Map...")) openFile();
        }
        std::string groupPreview = auditGroup_ ? Strings::fourccToString(*auditGroup_)
            : auditExcludeBsp_ ? "All but sbsp" : "All tag groups";
        if (ImGui::BeginCombo("Tag group", groupPreview.c_str())) {
            if (ImGui::Selectable("All tag groups", !auditGroup_ && !auditExcludeBsp_)) {
                auditGroup_.reset();
                auditExcludeBsp_ = false;
            }
            if (ImGui::Selectable("All but sbsp", !auditGroup_ && auditExcludeBsp_)) {
                auditGroup_.reset();
                auditExcludeBsp_ = true;
            }
            std::set<uint32_t> groups;
            if (map_) {
                for (uint32_t i = 0; i < map_->getTagCount(); ++i) {
                    auto* tag = map_->getTag(i);
                    if (tag) groups.insert(tag->groupID);
                }
            }
            if (auto* schema = Engine::Map::getMainSchema()) {
                for (const auto& [id, structure] : schema->structures)
                    if (structure.groupId != Engine::GroupId_Invalid) groups.insert(structure.groupId);
            }
            for (const auto& map : results.maps)
                for (const auto& [id, group] : map.analysis.groups) groups.insert(id);
            if (auditGroup_) groups.insert(*auditGroup_);
            for (auto group : groups) {
                bool selected = auditGroup_ == group;
                auto label = Strings::fourccToString(group);
                if (ImGui::Selectable(label.c_str(), selected)) {
                    auditGroup_ = group;
                    auditExcludeBsp_ = false;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Checkbox("Ignore alignment gaps", &auditOptions_.ignoreAlignmentGaps);
        ImGui::SetItemTooltip("Ignore gaps of 3 bytes or fewer that end on a 4-byte aligned address.");
        ImGui::Checkbox("Exclude tags with missing schema", &auditExcludeMissingSchema_);
        ImGui::SetItemTooltip("Skip tag types with no root schema or no mapped fields.");
        ImGui::BeginDisabled(auditDiskMaps_ ? auditPaths_.empty() : !map_);
        bool start = ImGui::Button(audit_ ? "Run Audit Again" : "Start Analysis");
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (running) {
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) audit_->cancel();
        }

        ImGui::TextWrapped(auditDiskMaps_
            ? "Scans disk files sequentially using one schema snapshot. Unsaved map edits are not included."
            : "Audits a snapshot of the current map and schema, including unsaved edits.");
        ImGui::TextWrapped("Links open independent inspection copies without replacing the main map. "
            "Results are not live; rerun after edits. Changed or missing disk files must be rescanned.");
        if (!auditError_.empty()) ImGui::TextWrapped("%s", auditError_.c_str());
        if (audit_ && results.status == AuditStatus::Cancelled)
            ImGui::TextUnformatted("Cancelled. Results below are partial.");
        if (audit_ && results.status == AuditStatus::Completed)
            ImGui::Text("Audit complete. %zu maps failed to load or scan.", results.failedMaps);
        ImGui::Text("%zu problems counted; %zu / %zu stored; %zu tags could not be analyzed",
            results.problemCount, results.storedProblemCount, auditProblemLimit, results.errorCount);
        if (results.excludedTags)
            ImGui::TextDisabled("%zu tags excluded due to missing schema", results.excludedTags);
        if (results.problemCount > results.storedProblemCount)
            ImGui::TextWrapped("Storage limit reached. Counts continue; some tags have counts but no stored details.");

        ImGui::Separator();
        const ImVec4 cleanColor(0.35f, 0.8f, 0.4f, 1.0f);
        if (ImGui::BeginChild("Audit results", ImVec2(0, 0), ImGuiChildFlags_None,
            ImGuiWindowFlags_HorizontalScrollbar)) {
            for (size_t mapIndex = 0; mapIndex < results.maps.size(); ++mapIndex) {
                const auto& map = results.maps[mapIndex];
                const auto& analysis = map.analysis;
                const char* status = analysis.status == AuditStatus::Pending ? "queued"
                    : analysis.status == AuditStatus::Running ? "analyzing"
                    : analysis.status == AuditStatus::Cancelled ? "cancelled"
                    : analysis.status == AuditStatus::Failed ? "failed" : "complete";
                ImGui::PushID(static_cast<int>(mapIndex));
                bool mapClean = analysis.status == AuditStatus::Completed &&
                    !analysis.problemCount && !analysis.errorCount && analysis.error.empty();
                if (mapClean) ImGui::PushStyleColor(ImGuiCol_Text, cleanColor);
                bool mapOpen = ImGui::TreeNodeEx("Map", ImGuiTreeNodeFlags_DefaultOpen,
                    "%s%s - %s; %zu problems, %zu / %zu tags",
                    map.path.filename().string().c_str(), map.fromSnapshot ? " (snapshot)" : "",
                    status, analysis.problemCount, analysis.completedTags, analysis.totalTags);
                if (mapClean) ImGui::PopStyleColor();
                ImGui::SetItemTooltip("%s", map.path.string().c_str());
                if (mapOpen) {
                    if (!analysis.error.empty()) ImGui::TextWrapped("Map failed: %s", analysis.error.c_str());
                    if (analysis.excludedTags) ImGui::TextDisabled("%zu tags excluded", analysis.excludedTags);
            for (const auto& [groupId, group] : analysis.groups) {
                auto groupName = Strings::fourccToString(groupId);
                ImGui::PushID(static_cast<int>(groupId));
                bool groupClean = analysis.status == AuditStatus::Completed &&
                    analysis.error.empty() && !group.problemCount && !group.errorCount;
                if (groupClean) ImGui::PushStyleColor(ImGuiCol_Text, cleanColor);
                bool open = ImGui::TreeNode("Group", "[%s] %zu problems (%zu stored), %zu tags, %zu errors",
                    groupName.c_str(), group.problemCount, group.storedProblemCount,
                    group.tags.size(), group.errorCount);
                if (groupClean) ImGui::PopStyleColor();
                if (open) {
                    for (auto index : group.tags) {
                        const auto& tag = analysis.tags[index];
                        if (!tag.problemCount && tag.error.empty()) continue;
                        ImGui::PushID(static_cast<int>(tag.tagId));
                        bool expanded = ImGui::TreeNode("Tag", "%zu problems (%zu stored)",
                            tag.problemCount, tag.problems.size());
                        ImGui::SameLine();
                        auto label = tag.path.empty() ? std::to_string(tag.tagId) : tag.path;
                        if (ImGui::TextLink(label.c_str()))
                            openAuditTag(mapIndex, tag.tagId);
                        if (expanded) {
                            if (!tag.error.empty())
                                ImGui::TextWrapped("Not analyzed: %s", tag.error.c_str());
                            for (size_t i = 0; i < tag.problems.size(); ++i) {
                                const auto& problem = tag.problems[i];
                                ImGui::PushID(static_cast<int>(i));
                                char detail[160];
                                snprintf(detail, sizeof(detail), "%s - map offsets 0x%zX - 0x%zX",
                                    problemKindName(problem.kind), problem.startOffset, problem.endOffset);
                                if (ImGui::TextLink(detail))
                                    openAuditTag(mapIndex, tag.tagId);
                                ImGui::PopID();
                            }
                            if (tag.problemCount > tag.problems.size())
                                ImGui::TextDisabled("%zu additional problems counted, not stored",
                                    tag.problemCount - tag.problems.size());
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    if (!group.problemCount && !group.errorCount)
                        ImGui::TextUnformatted("No problems found in analyzed tags.");
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::End();
        // Keep this frame's result links paired with the job that produced them.
        if (start) startAudit();
    }

}
