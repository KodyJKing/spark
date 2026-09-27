#include "DissectTagNew.hpp"

#include "State.hpp"
#include "Functions.hpp"

#include "engine/map/schema/reference.hpp"
#include "engine/tags/tag_group_id.hpp"

#include "imgui.h"

#define DEBUG

#ifdef DEBUG
#include "utils/Debugging.hpp"
#include <iostream>
#define LOG(X) std::cout << "[DissectTagNew] " << X << std::endl;
#else
#define LOG(X)
#endif

namespace Mod::DevTools::DissectTagNew {

    using namespace Engine::Map;

    constexpr size_t kMaxDisplayableSize = 4096 << 2;
    constexpr size_t kBytesPerRow = 16;
    
    void openWindow(MapFile* map, uint32_t tagId) {
        state.openWindow(map, tagId);
    }

    void renderRowAddress(uintptr_t address) {
        ImGui::NewLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(128, 128, 128, 255));
        ImGui::Text("%p", (void*)address);
        ImGui::SameLine();
        ImGui::PopStyleColor();
    }

    void renderUntypedRow(StructureRef& structure, uint8_t* address, size_t length) {
        if (length == 0) {
            return;
        }
        
        bool error = false;
        size_t originalLength = length;
        if (length >= kMaxDisplayableSize) {
            error = true;
            length = kMaxDisplayableSize;
        }

        if (error) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        }

        renderRowAddress((uintptr_t)address);
        
        // Pad for 0x4 byte alignment
        size_t pad = (uintptr_t)address & 0x3;
        for (size_t i = 0; i < pad; ++i) {
            ImGui::Text("  ");
            ImGui::SameLine();
        }
        
        for (size_t i = 0; i < length; ++i) {
            uint8_t* byteAddress = address + i;
            
            bool stripe = ((uintptr_t)byteAddress) >> 2 & 1;
            if (stripe) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 100, 100, 255));
            }

            ImGui::Text("%02X", *byteAddress);

            // Tooltip
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Address: %p", (void*)byteAddress);
                ImGui::Text("Offset: 0x%zx", i);
                ImGui::Text("Value: 0x%02X", *byteAddress);
                if (originalLength != length) {
                    ImGui::Text("Bad length provided: 0x%zx", originalLength);
                }
                ImGui::EndTooltip();
            }

            // On right click, create a new field here.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                LOG("Creating new field at address: " << (void*)byteAddress);
                structure.createFieldAt(byteAddress);
            }

            ImGui::SameLine();

            if (stripe) {
                ImGui::PopStyleColor();
            }

            bool overflow = i % kBytesPerRow == kBytesPerRow - 1;
            bool isFinal = i == length - 1;
            if (overflow && !isFinal) {
                renderRowAddress((uintptr_t)(address + i + 1));
            }
        }

        if (error) {
            ImGui::PopStyleColor();
        }
    }

    void renderField(FieldRef& field) {
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

        // renderRowAddress has the sole responsibility of creating new lines.
        ImGui::SameLine();
    }

    void renderStructure(StructureRef& structure, size_t fallbackSize = 0) {
        #ifdef DEBUG
        if (ImGui::Button("Debug")) {
            Debugging::debug();
        }
        #endif
        
        if (!structure.valid()) {
            ImGui::Text("[invalid structure]");
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
            renderUntypedRow(structure, (uint8_t*)head, (uintptr_t)fieldRef.address - head);
            renderField(fieldRef);
            head = newHead;
        }
        renderUntypedRow(structure, (uint8_t*)head, structSize - (head - (uintptr_t)structure.address));

        ImGui::PopID();
    }

    void renderWindow(WindowState& window) {
        // Implement the rendering logic for a single window
        if (!window.isOpen) return;
        auto tag = window.getTag();
        auto path = window.map->getTagPath(tag);
        ImGui::Begin(path, &window.isOpen);
        if (!tag) {
            ImGui::Text("[invalid tag]");
        } else {
            Context ctx = { window.map, &state.schema };
            auto ref = window.getStructureRef(ctx);
            if (!ref.valid()) {
                ImGui::Text("[invalid structure reference]");
            } else {
                size_t fallbackSize = window.map->guessTagDataSize(tag->tagID);
                ImGui::Text("Estimated tag data size: %zu", fallbackSize);
                renderStructure(ref, fallbackSize);
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
