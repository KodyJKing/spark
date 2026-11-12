#include "InstructionAccessWindow.hpp"
#ifndef SPOOL
#include "State.hpp"
#include "Minimap.hpp"
#include <imgui.h>
#include <algorithm>
#include <stdexcept>

namespace SparkLib::UI::DissectTag {
    using namespace Engine::Map;

    void renderInstructionWatchButton(State& state, WindowState& window, void* tagData, size_t size) {
        ImGui::BeginDisabled(!tagData || size == 0);
        if (ImGui::Button("Find what instructions access this tag")) {
            window.instructionWatchError.clear();
            if (!window.instructionWatch) {
                try {
                    for (auto& [key, other] : state.windows) {
                        if (other.instructionWatch && other.instructionWatch->address() == tagData &&
                            other.instructionWatch->size() == size)
                            throw std::runtime_error("This memory range is already watched in another tag window.");
                    }
                    window.instructionWatch = std::make_unique<InstructionAccessWatch>(tagData, size);
                } catch (const std::exception& error) {
                    window.instructionWatchError = error.what();
                }
            }
            window.instructionWatchFocusRequested = true;
        }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip(size
            ? "Watches the estimated total tag range, including unparsed bytes. External payloads are not watched.\n"
              "Uses page guards: can slow the process and miss concurrent accesses. Closing the results unwatches."
            : "Cannot watch: the tag has no bounded total-size estimate.");
        if (!window.instructionWatchError.empty())
            ImGui::TextWrapped("Instruction watch: %s", window.instructionWatchError.c_str());
    }

    void renderInstructionAccessWindow(WindowState& window, bool interactive) {
        if (!window.instructionWatch || !window.isOpen)
            return;
        auto& watch = *window.instructionWatch;
        watch.poll();
        char identity[96];
        snprintf(identity, sizeof(identity), "###TagAccess:%p:%08X", window.map, window.tagId);
        auto* tag = window.getTag();
        auto* path = tag ? window.map->getTagPath(tag) : nullptr;
        auto title = std::string("Instructions accessing ") + (path ? path : "[invalid tag]") + identity;
        ImGui::SetNextWindowSize(ImVec2(1100, 420), ImGuiCond_FirstUseEver);
        if (interactive && window.instructionWatchFocusRequested) {
            ImGui::SetNextWindowFocus();
            window.instructionWatchFocusRequested = false;
        }
        bool open = true;
        ImGuiWindowFlags flags = interactive ? ImGuiWindowFlags_None
            : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        if (ImGui::Begin(title.c_str(), &open, flags)) {
            ImGui::Text("Watching %p - %p (0x%zX bytes)", watch.address(),
                static_cast<char*>(watch.address()) + watch.size(), watch.size());
            ImGui::TextDisabled("Page guards are best-effort across threads. Dissect/minimap reads are excluded.");
            if (watch.droppedHits())
                ImGui::TextWrapped("Dropped %llu hits: capture queue was full or busy. Counts are incomplete.",
                    static_cast<unsigned long long>(watch.droppedHits()));
            if (!window.instructionWatchError.empty())
                ImGui::TextWrapped("Instruction watch: %s", window.instructionWatchError.c_str());
            if (watch.instructions().empty())
                ImGui::TextDisabled("Waiting for accesses...");

            std::vector<const InstructionAccess*> instructions;
            for (const auto& [address, instruction] : watch.instructions())
                instructions.push_back(&instruction);
            std::sort(instructions.begin(), instructions.end(), [](const auto* a, const auto* b) {
                return a->hits != b->hits ? a->hits > b->hits : a->instruction < b->instruction;
            });
            if (ImGui::BeginTable("Instructions", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Instruction address");
                ImGui::TableSetupColumn("Disassembly");
                ImGui::TableSetupColumn("Hits");
                ImGui::TableSetupColumn("Last accessed address");
                ImGui::TableSetupColumn("Parse span (last access)");
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(instructions.size()));
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const auto& instruction = *instructions[i];
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        char address[32];
                        snprintf(address, sizeof(address), "%016llX",
                            static_cast<unsigned long long>(instruction.instruction));
                        if (ImGui::Selectable(address))
                            ImGui::SetClipboardText(address);
                        ImGui::SetItemTooltip("Click to copy the instruction address.");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(instruction.disassembly.c_str());
                        ImGui::SetItemTooltip("%s", instruction.disassembly.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", static_cast<unsigned long long>(instruction.hits));
                        ImGui::TableNextColumn();
                        ImGui::Text("%016llX", static_cast<unsigned long long>(instruction.lastAddress));
                        if (instruction.lastSize)
                            ImGui::SetItemTooltip("Access size: 0x%zX bytes", instruction.lastSize);
                        else
                            ImGui::SetItemTooltip("Access size could not be decoded.");
                        ImGui::TableNextColumn();
                        const auto* span = window.minimap && window.minimap->error.empty()
                            ? findInstructionAccessSpan(instruction, window.minimap->overlaps) : nullptr;
                        if (span) {
                            const auto start = reinterpret_cast<uintptr_t>(span->start);
                            auto offset = instruction.lastAddress > start ? instruction.lastAddress - start : 0;
                            auto index = span->index;
                            if (index < 0 && span->count && span->type && span->type->size) {
                                index = static_cast<int64_t>(offset / span->type->size);
                                offset %= span->type->size;
                            }
                            const char* name = span->type
                                ? (span->type->name[0] ? span->type->name : "[unnamed]")
                                : TypeNames[static_cast<size_t>(span->fieldType)];
                            ImGui::Text("%s%s +0x%zX", name,
                                minimapIndexLabel(index, span->count).c_str(), offset);
                            ImGui::SetItemTooltip("%s\nSpan: %p - %p\nTag offset: 0x%zX",
                                name, static_cast<void*>(span->start), static_cast<void*>(span->end),
                                start - reinterpret_cast<uintptr_t>(watch.address()));
                        } else {
                            if (window.minimap && !window.minimap->error.empty()) {
                                ImGui::TextDisabled("(parse analysis failed)");
                                ImGui::SetItemTooltip("%s", window.minimap->error.c_str());
                            } else {
                                ImGui::TextDisabled(window.minimap ? "(no span)" : "(no parse snapshot)");
                            }

                        }
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
        if (!open) {
            try {
                watch.stop();
                window.instructionWatch.reset();
                window.instructionWatchError.clear();
            } catch (const std::exception& error) {
                window.instructionWatchError = std::string("Could not unwatch: ") + error.what();
            }
        }
    }

    void renderInstructionAccessWindows(State& state, bool interactive) {
        ScopedAccessCaptureSuppression suppress;
        state.cleanupWindows();
        for (auto& [key, window] : state.windows)
            if (window.isRoot()) renderInstructionAccessWindow(window, interactive);
        state.cleanupWindows();
    }
}
#endif
