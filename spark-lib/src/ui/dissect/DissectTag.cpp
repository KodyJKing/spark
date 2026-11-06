#include "DissectTag.hpp"

#include "Constants.hpp"
#include "State.hpp"
#include "Functions.hpp"
#include "Hints.hpp"
#include "Untyped.hpp"
#include "Substructure.hpp"
#include "Minimap.hpp"

#include "engine/map/schema/reference.hpp"
#include "engine/tags/tag_group_id.hpp"
#include "engine/map/schema/functions.hpp"

#include "utils/Strings.hpp"

#include "imgui.h"

#include <algorithm>
namespace SparkLib::UI::DissectTag {

    using namespace Engine::Map;

    void openWindow(MapFile* map, uint32_t tagId) {
        state.openWindow(map, tagId);
    }

    void openStructureWindow(MapFile* map, uint32_t tagId, size_t offset, const std::string& structureId, int64_t elementIndex) {
        state.openWindow(map, tagId, static_cast<int64_t>(offset), structureId, elementIndex);
    }
    
    std::vector<uint32_t> openTagIds() {
        std::vector<uint32_t> ids;
        // Structure windows aren't restored across sessions.
        for (auto& [key, window] : state.windows)
            if (window.isOpen && window.isRoot()) ids.push_back(window.tagId);
        return ids;
    }

    void closeWindowsForMap(MapFile* map) {
        for (auto& [key, window] : state.windows) {
            if (window.map == map) window.isOpen = false;
        }
        state.cleanupWindows();
    }

    void renderFieldChildren(RenderContext& renderCtx, FieldRef& field) {

        if (field.is(Type::Structure)) {
            StructureRef substructure = field.getStructure();
            renderSubstructure(renderCtx, substructure, field, 0x100);
        }
        
        if (field.is(Type::BlockPointer)) {

            ImGui::PushID(field.address);
            // Index
            auto storage = ImGui::GetStateStorage();
            std::string indexKey = "BlockIndex_" + std::to_string((uintptr_t)field.address);
            ImGuiID indexId = ImGui::GetID(indexKey.c_str());
            int index = storage->GetInt(indexId, 0);

            
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
            ImGui::InputInt("Index", &index, 1, 10);

            BlockPointer* blockPointer = field.getBlockPointer();
            if (index >= blockPointer->count) {
                index = blockPointer->count - 1;
            } else if (index < 0) {
                index = 0;
            }
            
            storage->SetInt(indexId, index);
            ImGui::PopID();
            
            StructureRef blockElement = field.getBlockElement(index);
            renderSubstructure(renderCtx, blockElement, field, 0x100);
        }

    }

    void renderField(RenderContext& renderCtx, FieldRef& field) {
        if (!field.valid()) {
            ImGui::Text("[invalid field reference]");
            return;
        }

        renderRowAddress((uintptr_t)field.address, (uintptr_t)field.parent.address);

        ImGui::PushID(field.address);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16);
        ImGui::InputText("##Name", field.node->name, 256);

        ImGui::SameLine();
        renderTypePicker(&field.node->type.type);

        if (field.is(Type::Structure) || field.is(Type::BlockPointer)) {
            ImGui::SameLine();
            if (renderStructureTypePicker(field))
                renderCtx.windowState->minimapRefreshRequested = true;
        }

        std::string valueStr = field.readAsString();
        ImGui::SameLine();
        ImGui::Text("%s", valueStr.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("Edit Field");
        }

        if (ImGui::BeginPopup("Edit Field")) {
            static char buffer[256] = "";
            ImGui::InputText("Value", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button("Set")) {
                field.writeFromString(buffer);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("x")) {
            field.deleteField();
        }

        ImGui::PopID();

        // Only renderRowAddress may create new lines.
        ImGui::SameLine();

        renderFieldChildren(renderCtx, field);
    }
    
    void renderStructure(RenderContext& renderCtx, StructureRef& structure, size_t fallbackSize = 0) {
        if (!structure.valid()) {
            ImGui::Text("[invalid structure]");
            return;
        }
        if (!structure.allocated()) {
            ImGui::Text("[unallocated memory]");
            return;
        }

        ImGui::PushID(structure.address);

        size_t structSize = structure.node->size;
        if (structSize == 0) structSize = fallbackSize;
        if (structSize > kMaxDisplayableSize) structSize = kMaxDisplayableSize;

        uintptr_t head = (uintptr_t)structure.address;
        auto fieldRefs = structure.getFieldRefs();
        for (auto& fieldRef : fieldRefs) {
            uintptr_t newHead = (uintptr_t)fieldRef.address + fieldRef.size();
            renderUntypedRow(renderCtx, structure, (uint8_t*)head, (uintptr_t)fieldRef.address - head);
            renderField(renderCtx, fieldRef);
            head = newHead;
        }
        size_t renderedSize = head - (uintptr_t)structure.address;
        if (renderedSize < structSize)
            renderUntypedRow(renderCtx, structure, (uint8_t*)head, structSize - renderedSize);

        ImGui::PopID();
    }

    void windowHeader() {
        if(ImGui::Button("Save")) state.saveSchema();
        ImGui::SameLine();
        
        if (ImGui::CollapsingHeader("Options")) {
            // Slider for hint threshold
            ImGui::Text("Hint Threshold");
            ImGui::SameLine();
            ImGui::SliderFloat("##HintThreshold", &state.hintThreshold, 0.01f, 1.0f);

        }
    }

    constexpr float kMinDrawerHeight = 80.0f;
    constexpr float kDrawerHandleHeight = 6.0f;

    // One structure view: its offset, an editable size, and the structure's fields.
    void renderStructureTab(WindowState& root, WindowState& view, Context& ctx, void* tagData, void* tagEnd) {
        auto ref = view.getStructureRef(ctx);
        if (!ref.valid()) {
            ImGui::Text("[invalid structure reference]");
            return;
        }
        if (!Memory::isAllocated(ref.address)) {
            ImGui::Text("[memory not allocated]");
            return;
        }

        ImGui::Text("Offset in tag: 0x%llX", (unsigned long long)view.structureOffset);
        if (!ref.node->readonly()) {
            ImGui::SameLine();
            uint64_t size = ref.node->size;
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
            if (ImGui::InputScalar("Size", ImGuiDataType_U64, &size,
                nullptr, nullptr, "%llX", ImGuiInputTextFlags_CharsHexadecimal)) {
                ref.node->size = static_cast<size_t>(size);
                root.minimapRefreshRequested = true;
            }
            ImGui::SetItemTooltip("Shared by every use of this structure. Use Save to persist.");
        }

        size_t remaining = (uint8_t*)tagEnd > (uint8_t*)ref.address ? (uint8_t*)tagEnd - (uint8_t*)ref.address : 0;
        RenderContext renderCtx = { &view, tagData, tagEnd };
        ImGui::BeginChild("StructureScroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        renderStructure(renderCtx, ref, (std::min)(remaining, (size_t)0x100));
        ImGui::EndChild();
    }

    // Tabbed drawer under a tag's structure view, with a draggable divider above it.
    void renderStructureDrawer(
        WindowState& root, std::vector<WindowState*>& views, Context& ctx, void* tagData, void* tagEnd
    ) {
        ImVec2 handleStart = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        ImGui::InvisibleButton("##drawerResize", ImVec2(width, kDrawerHandleHeight));
        bool resizing = ImGui::IsItemActive();
        bool hovered = ImGui::IsItemHovered();
        if (resizing || hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (resizing) root.drawerHeight -= ImGui::GetIO().MouseDelta.y;
        float y = handleStart.y + kDrawerHandleHeight * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(handleStart.x, y), ImVec2(handleStart.x + width, y),
            ImGui::GetColorU32(resizing ? ImGuiCol_SeparatorActive
                : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator), 2.0f);

        ImGui::BeginChild("StructureDrawer", ImVec2(0, 0), ImGuiChildFlags_None);
        if (ImGui::BeginTabBar("StructureTabs",
            ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
            for (auto* view : views) {
                auto ref = view->getStructureRef(ctx);
                const char* name = ref.node && ref.node->name[0] ? ref.node->name : "[unnamed]";
                char index[32] = "";
                if (view->elementIndex >= 0)
                    snprintf(index, sizeof(index), "[%lld]", (long long)view->elementIndex);
                // The ### suffix keeps the tab identity stable across renames.
                char label[320];
                snprintf(label, sizeof(label), "%s%s @0x%llX###%lld", name, index,
                    (unsigned long long)view->structureOffset, (long long)view->structureOffset);
                ImGuiTabItemFlags flags = view->focusRequested ? ImGuiTabItemFlags_SetSelected : 0;
                view->focusRequested = false;
                if (ImGui::BeginTabItem(label, &view->isOpen, flags)) {
                    renderStructureTab(root, *view, ctx, tagData, tagEnd);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
    }

    void renderWindow(WindowState& window) {
        // Implement the rendering logic for a single window
        if (!window.isOpen) return;
        auto tag = window.getTag();
        auto path = window.map->getTagPath(tag);

        auto groupId = Strings::fourccToString(tag->groupID);
        char title[256];
        snprintf(title, sizeof(title), "[%s] %s - %p", groupId.c_str(), path, tag);
        
        if (window.focusRequested) {
            ImGui::SetNextWindowFocus();
            window.focusRequested = false;
        }
        ImGui::Begin(title, &window.isOpen);
        windowHeader();
        if (!tag) {
            ImGui::Text("[invalid tag]");
        } else {
            Context ctx = { window.map, state.schema };
            auto ref = window.getStructureRef(ctx);
            if (ref.node && !ref.node->readonly()) {
                ImGui::Checkbox("Tag type certified complete", &ref.node->complete);
                ImGui::SetItemTooltip(
                    "Certifies all relocation-sensitive fields, including nested structures, "
                    "for every tag of this type. Use Save to persist this setting.");
            }
            if (!ref.valid()) {
                ImGui::Text("[invalid structure reference]");
            } else if (!Memory::isAllocated(ref.address)) {
                ImGui::Text("[memory not allocated]");
            } else {
                size_t guessedTotalSize = window.map->guessTagDataSize(tag->tagHandle);
                size_t firstChildSize = Engine::Map::guessSizeFromFirstBlockElement(ref);
                size_t guessedSize = firstChildSize ? firstChildSize : guessedTotalSize;

                if (!ref.node->readonly()) {
                    uint64_t size = ref.node->size;
                    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
                    if (ImGui::InputScalar("Size", ImGuiDataType_U64, &size,
                        nullptr, nullptr, "%llX", ImGuiInputTextFlags_CharsHexadecimal)) {
                        ref.node->size = static_cast<size_t>(size);
                        window.minimapRefreshRequested = true;
                    }
                    ImGui::SetItemTooltip(
                        "Shared by all tags of this type. Zero leaves the size unspecified. Use Save to persist.");
                    if (firstChildSize > 0) {
                        ImGui::SameLine();
                        char label[64];
                        snprintf(label, sizeof(label), "Use guess: %zX", firstChildSize);
                        if (ImGui::SmallButton(label)) {
                            ref.node->size = firstChildSize;
                            window.minimapRefreshRequested = true;
                        }
                    }
                    ImGui::SameLine();
                }

                void* tagData = window.map->getTagData(tag);
                void* tagEnd = (uint8_t*)tagData + guessedTotalSize;
                RenderContext renderCtx = { &window, tagData, tagEnd };

                if (firstChildSize > 0)
                    ImGui::Text("Size estimate: base-structure %zX, total %zX", firstChildSize, guessedTotalSize);
                else
                    ImGui::Text("Size estimate: base-structure unknown, total %zX", guessedTotalSize);

                if (ImGui::CollapsingHeader("Parse Minimap", ImGuiTreeNodeFlags_DefaultOpen)) {
                    renderMinimap(renderCtx, ref);
                }

                // Structure views opened from the minimap live in a drawer at the bottom of this window.
                std::vector<WindowState*> views;
                for (auto it = state.windows.lower_bound(WindowKey{window.tagId, 0});
                     it != state.windows.end() && it->first.first == window.tagId; ++it)
                    if (it->second.isOpen) views.push_back(&it->second);

                float scrollHeight = 0.0f;
                if (!views.empty()) {
                    float avail = ImGui::GetContentRegionAvail().y;
                    window.drawerHeight = (std::max)(kMinDrawerHeight,
                        (std::min)(window.drawerHeight, avail - kMinDrawerHeight - kDrawerHandleHeight));
                    scrollHeight = (std::max)(1.0f, avail - window.drawerHeight - kDrawerHandleHeight);
                }

                ImGui::BeginChild("StructureScroll", ImVec2(0, scrollHeight), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
                renderStructure(renderCtx, ref, guessedSize);
                ImGui::EndChild();

                if (!views.empty())
                    renderStructureDrawer(window, views, ctx, tagData, tagEnd);
            }
        }

        ImGui::End();
    }

    void render() {
        state.tick();
        for (auto& [key, window] : state.windows) {
            // Structure views render as tabs inside their tag's window.
            if (window.isRoot()) renderWindow(window);
        }
    }
    
}
