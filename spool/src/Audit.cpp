#include "App.hpp"

#include "engine/map/schema/schema_file.hpp"
#include "ui/dissect/DissectTag.hpp"
#include "utils/Strings.hpp"

#include <set>
#include <stdexcept>

namespace Spool {

    void App::startAudit() {
        try {
            auto* schema = Engine::Map::getMainSchema();
            if (!schema) throw std::runtime_error("No tag schema is loaded");
            audit_ = std::make_unique<Engine::Map::Analysis::AuditJob>(
                map_->snapshot(), *schema, auditGroup_, auditOptions_);
            auditError_.clear();
        } catch (const std::exception& error) {
            auditError_ = std::string("Could not start audit: ") + error.what();
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

        AuditResults results;
        if (audit_) results = audit_->snapshot();
        bool running = audit_ && results.status == AuditStatus::Running;
        char progress[128];
        snprintf(progress, sizeof(progress), "%zu / %zu tags%s",
            results.completedTags, results.totalTags, running ? " - analyzing" : "");
        float fraction = results.totalTags
            ? static_cast<float>(results.completedTags) / static_cast<float>(results.totalTags) : 0.0f;
        if (audit_ && results.status == AuditStatus::Completed && !results.totalTags) fraction = 1.0f;
        ImGui::ProgressBar(fraction, ImVec2(-1, 0), audit_ ? progress : "Not started");

        if (!map_) {
            ImGui::TextUnformatted("Open a map to audit its tags.");
            if (ImGui::Button("Open Map...")) openFile();
            ImGui::End();
            return;
        }

        ImGui::TextWrapped("Map: %s", map_->getFilePath().string().c_str());
        ImGui::BeginDisabled(running);
        std::string groupPreview = auditGroup_ ? Strings::fourccToString(*auditGroup_) : "All tag groups";
        if (ImGui::BeginCombo("Tag group", groupPreview.c_str())) {
            if (ImGui::Selectable("All tag groups", !auditGroup_)) auditGroup_.reset();
            std::set<uint32_t> groups;
            for (uint32_t i = 0; i < map_->getTagCount(); ++i) {
                auto* tag = map_->getTag(i);
                if (tag) groups.insert(tag->groupID);
            }
            for (auto group : groups) {
                bool selected = auditGroup_ == group;
                auto label = Strings::fourccToString(group);
                if (ImGui::Selectable(label.c_str(), selected)) auditGroup_ = group;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Checkbox("Ignore gaps after DataPointers", &auditOptions_.ignoreGapsAfterDataPointer);
        bool start = ImGui::Button(audit_ ? "Run Audit Again" : "Start Analysis");
        ImGui::EndDisabled();
        if (start) startAudit();
        if (running) {
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) audit_->cancel();
        }

        ImGui::TextWrapped("Audits a snapshot of the open map and schema, including unsaved edits. "
            "Results appear as tags finish. Rerun after edits to update the audit.");
        if (!auditError_.empty()) ImGui::TextWrapped("%s", auditError_.c_str());
        if (!results.error.empty()) ImGui::TextWrapped("Audit failed: %s", results.error.c_str());
        if (audit_ && results.status == AuditStatus::Cancelled)
            ImGui::TextUnformatted("Cancelled. Results below are partial.");
        if (audit_ && results.status == AuditStatus::Completed)
            ImGui::TextUnformatted("Audit complete.");
        ImGui::Text("%zu problems counted; %zu / %zu stored; %zu tags could not be analyzed",
            results.problemCount, results.storedProblemCount, auditProblemLimit, results.errorCount);
        if (results.problemCount > results.storedProblemCount)
            ImGui::TextWrapped("Storage limit reached. Counts continue; some tags have counts but no stored details.");

        ImGui::Separator();
        if (ImGui::BeginChild("Audit results", ImVec2(0, 0), ImGuiChildFlags_None,
            ImGuiWindowFlags_HorizontalScrollbar)) {
            for (const auto& [groupId, group] : results.groups) {
                auto groupName = Strings::fourccToString(groupId);
                ImGui::PushID(static_cast<int>(groupId));
                bool open = ImGui::TreeNode("Group", "[%s] %zu problems (%zu stored), %zu tags, %zu errors",
                    groupName.c_str(), group.problemCount, group.storedProblemCount,
                    group.tags.size(), group.errorCount);
                if (open) {
                    for (auto index : group.tags) {
                        const auto& tag = results.tags[index];
                        if (!tag.problemCount && tag.error.empty()) continue;
                        ImGui::PushID(static_cast<int>(tag.tagId));
                        bool expanded = ImGui::TreeNode("Tag", "%zu problems (%zu stored)",
                            tag.problemCount, tag.problems.size());
                        ImGui::SameLine();
                        auto label = tag.path.empty() ? std::to_string(tag.tagId) : tag.path;
                        if (ImGui::TextLink(label.c_str()))
                            SparkLib::UI::DissectTag::openWindow(map_.get(), tag.tagId);
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
                                    SparkLib::UI::DissectTag::openWindow(map_.get(), tag.tagId);
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
        }
        ImGui::EndChild();
        ImGui::End();
    }

}
