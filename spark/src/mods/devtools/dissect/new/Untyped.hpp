#pragma once

#include "State.hpp"
#include "Constants.hpp"
#include "Hints.hpp"
#include "Functions.hpp"
#include "imgui.h"

namespace Mod::DevTools::DissectTagNew {

    inline void renderUntypedRow(RenderContext& renderCtx, StructureRef& structure, uint8_t* address, int32_t length) {
        if (length <= 0) {
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
        size_t pad = (uintptr_t)address & 0b11;
        for (size_t i = 0; i < pad; ++i) {
            ImGui::Text("  ");
            ImGui::SameLine();
        }
        
        for (size_t i = 0; i < length; ++i) {
            uint8_t* byteAddress = address + i;
            uint32_t offset = (uintptr_t)byteAddress - (uintptr_t)structure.address;

            bool stripe = ((uintptr_t)offset) >> 2 & 1;
            uint32_t stripeColor = kStripeColor;
            if (stripe) ImGui::PushStyleColor(ImGuiCol_Text, stripeColor);

            // Compute scores
            Hints::HintContext hintCtx = {
                renderCtx.tagStart,
                renderCtx.tagEnd,
                structure
            };
            
            float* scores = renderCtx.windowState->hintCache.getScores(hintCtx, byteAddress);
            bool hasNotableScore = false;
            for (int i = 0; i < static_cast<int>(Hints::TypeCount); ++i) {
                if (scores[i] >= state.hintThreshold) {
                    hasNotableScore = true;
                    break;
                }
            }
            if (hasNotableScore) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 64, 255));
            }

            ImGui::Text("%02X", *byteAddress);

            // Tooltip
            bool showTooltip = ImGui::IsItemHovered();

            // On right click, create a new field here.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                LOG("Creating new field at address: " << (void*)byteAddress);
                structure.createFieldAt(byteAddress);
            }

            ImGui::SameLine();

            if (hasNotableScore) ImGui::PopStyleColor();
            if (stripe) ImGui::PopStyleColor();

            if (showTooltip) {
                ImGui::BeginTooltip();
                ImGui::Text("Address: %p", (void*)byteAddress);
                ImGui::Text("Offset: 0x%zx", offset);
                Hints::renderHints(hintCtx, byteAddress, state.hintThreshold);
                ImGui::EndTooltip();
            }


            bool overflow = (offset + pad) % kBytesPerRow == kBytesPerRow - 1;
            bool isFinal = i == length - 1;
            if (overflow && !isFinal) {
                renderRowAddress((uintptr_t)(address + i + 1));
            }
        }

        if (error) {
            ImGui::PopStyleColor();
        }
    }

}