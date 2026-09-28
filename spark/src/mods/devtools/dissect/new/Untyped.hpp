#pragma once

#include "State.hpp"
#include "Constants.hpp"
#include "Hints.hpp"
#include "Functions.hpp"
#include "imgui.h"

#include <sstream>

namespace Mod::DevTools::DissectTagNew {

    inline void renderUntypedRow(RenderContext& renderCtx, StructureRef& structure, uint8_t* address, int32_t length) {
        if (length <= 0) return;
        if (length >= kMaxDisplayableSize) length = kMaxDisplayableSize;

        renderRowAddress((uintptr_t)address);
        
        // Pad for 0x4 byte alignment
        size_t pad = (uintptr_t)address & 0b11;
        for (size_t i = 0; i < pad; ++i) {
            ImGui::Text("  ");
            ImGui::SameLine();
        }

        std::stringstream textRow;
        auto printRowText = [&]() {
            std::string row = textRow.str();
            // ImGui::SameLine();
            // ImGui::Text(" | %s", row.c_str());
            textRow.str("");
            textRow.clear();
        };
        auto pushRowChar = [&](char c) {
            if (c < 32 || c > 126) c = '.';
            textRow << c;
        };
        
        for (size_t i = 0; i < length; ++i) {
            uint8_t* byteAddress = address + i;
            uint32_t offset = (uintptr_t)byteAddress - (uintptr_t)structure.address;

            bool stripe = ((uintptr_t)offset) >> 2 & 1;
            uint32_t stripeColor = kStripeColor;
            if (stripe) ImGui::PushStyleColor(ImGuiCol_Text, stripeColor);

            // Compute scores
            Hints::HintCache& hintCache = renderCtx.windowState->hintCache;
            Hints::HintContext hintCtx = {
                renderCtx.tagStart,
                renderCtx.tagEnd,
                structure
            };
            bool hintColored = hintCache.colorAddress(hintCtx, byteAddress, state.hintThreshold);

            ImGui::Text("%02X", *byteAddress);
            bool showTooltip = ImGui::IsItemHovered();
            pushRowChar(static_cast<char>(*byteAddress));

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !structure.node->readonly()) {
                LOG("Creating new field at address: " << (void*)byteAddress);
                Type suggested = hintCache.suggestedType(hintCtx, byteAddress, state.hintThreshold);
                structure.createFieldAt(byteAddress, suggested);
            }

            ImGui::SameLine();

            if (hintColored) ImGui::PopStyleColor();
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
            if (overflow) {
                printRowText();
                if (!isFinal) {
                    renderRowAddress((uintptr_t)(address + i + 1));
                }
            }
        }
    }

}
