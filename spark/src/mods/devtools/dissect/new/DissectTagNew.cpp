#include "DissectTagNew.hpp"

#include "Constants.hpp"
#include "State.hpp"
#include "Functions.hpp"
#include "Hints.hpp"
#include "Untyped.hpp"
#include "Substructure.hpp"

#include "engine/map/schema/reference.hpp"
#include "engine/tags/tag_group_id.hpp"
#include "engine/map/schema/functions.hpp"

#include "imgui.h"

namespace Mod::DevTools::DissectTagNew {

    using namespace Engine::Map;

    void openWindow(MapFile* map, uint32_t tagId) {
        state.openWindow(map, tagId);
    }

    void tryRenderBlock(RenderContext& renderCtx, FieldRef& field) {
        if (field.is(Type::BlockPointer)) {
            StructureRef blockElement = field.getBlockElement(0);
            renderSubstructure(renderCtx, blockElement, field, 0x100);
        }
    }
    
    void renderFieldChildren(RenderContext& renderCtx, FieldRef& field) {
        tryRenderBlock(renderCtx, field);
    }

    void renderField(RenderContext& renderCtx, FieldRef& field) {
        if (!field.valid()) {
            ImGui::Text("[invalid field reference]");
            return;
        }

        renderRowAddress((uintptr_t)field.address);

        ImGui::PushID(field.address);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        ImGui::InputText("##Name", field.node->name, 256);

        ImGui::SameLine();
        renderTypePicker(&field.node->type.type);

        std::string valueStr = field.readAsString();
        ImGui::SameLine();
        ImGui::Text("%s", valueStr.c_str());

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
        renderUntypedRow(renderCtx, structure, (uint8_t*)head, structSize - (head - (uintptr_t)structure.address));

        ImGui::PopID();
    }

    void windowHeader() {
        if (ImGui::CollapsingHeader("Options")) {
            // Slider for hint threshold
            ImGui::Text("Hint Threshold");
            ImGui::SameLine();
            ImGui::SliderFloat("##HintThreshold", &state.hintThreshold, 0.01f, 1.0f);

            ImGui::SameLine();
            #ifdef DEBUG
            if (ImGui::Button("Start Debugging")) {
                Debugging::debug();
            }
            #endif
        }
    }

    void renderWindow(WindowState& window) {
        // Implement the rendering logic for a single window
        if (!window.isOpen) return;
        auto tag = window.getTag();
        auto path = window.map->getTagPath(tag);
        ImGui::Begin(path, &window.isOpen);
        windowHeader();
        if (!tag) {
            ImGui::Text("[invalid tag]");
        } else {
            Context ctx = { window.map, &state.schema };
            auto ref = window.getStructureRef(ctx);
            if (!ref.valid()) {
                ImGui::Text("[invalid structure reference]");
            } else {
                size_t guessedTotalSize = window.map->guessTagDataSize(tag->tagID);
                size_t guessedSize = Engine::Map::guessSizeFromFirstBlockElement(ref);
                if (guessedSize == 0) 
                    guessedSize = guessedTotalSize;

                void* tagData = window.map->getTagData(tag);
                RenderContext renderCtx = { &window, tagData, (uint8_t*)tagData + guessedTotalSize };

                ImGui::Text("Estimated tag data size: %zu", guessedSize);
                renderStructure(renderCtx, ref, guessedSize);
            }
        }

        ImGui::End();
    }

    void render() {
        // Implement the rendering logic for the dissect tag window
        for (auto& [tagId, window] : state.windows) {
            renderWindow(window);
        }
        state.cleanupWindows();
    }
    
}
