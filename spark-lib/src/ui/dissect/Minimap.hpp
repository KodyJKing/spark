#pragma once

#include "State.hpp"
#include "engine/map/schema/span_map.hpp"
#include "imgui.h"

#include <cstdint>
#include <vector>

namespace SparkLib::UI::DissectTag {

    using namespace Engine::Map;

    // Rebuild the span cache at most once per this many seconds (plus on manual
    // refresh) so we don't walk the whole block tree every frame.
    constexpr double kMinimapRebuildSeconds = 1.0;
    constexpr float kMinimapHeight = 24.0f;
    constexpr uint32_t kMinimapBgColor = IM_COL32(28, 28, 28, 255);
    constexpr uint32_t kMinimapBorderColor = IM_COL32(80, 80, 80, 255);
    constexpr uint32_t kMinimapOverlapColor = IM_COL32(220, 60, 50, 255);

    // Stable color derived from the referenced type's identity, so repeated
    // structures share a hue and layout patterns are easy to spot.
    inline uint32_t minimapColorForType(StructureNode* type) {
        uint64_t h = (uint64_t)(uintptr_t)type;
        h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33;
        float hue = (h & 0xffff) / 65535.0f;
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hue, 0.55f, 0.85f, r, g, b);
        return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
    }

    inline void renderMinimap(RenderContext& renderCtx, StructureRef& root) {
        WindowState* window = renderCtx.windowState;

        if (ImGui::SmallButton("Refresh Minimap")) window->minimapBuiltTime = -1.0;

        double now = ImGui::GetTime();
        if (window->minimapBuiltTime < 0.0 ||
            now - window->minimapBuiltTime >= kMinimapRebuildSeconds) {
            window->minimapSpans.spans.clear();
            Analysis::fillSpanMap(root, window->minimapSpans, renderCtx.tagStart, renderCtx.tagEnd);
            window->minimapOverlaps = Analysis::analyzeOverlaps(window->minimapSpans);
            window->minimapBuiltTime = now;
        }

        char* tagStart = (char*)renderCtx.tagStart;
        char* tagEnd = (char*)renderCtx.tagEnd;
        double total = (double)(tagEnd - tagStart);
        if (total <= 0.0) {
            ImGui::TextDisabled("[minimap: empty tag range]");
            return;
        }

        float width = ImGui::GetContentRegionAvail().x;
        if (width < 1.0f) return;

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + width, p0.y + kMinimapHeight);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(p0, p1, kMinimapBgColor);

        for (auto& info : window->minimapOverlaps) {
            char* s = info.span.start;
            char* e = info.span.end;
            if (e <= tagStart || s >= tagEnd) continue;
            if (s < tagStart) s = tagStart;
            if (e > tagEnd) e = tagEnd;

            float x0 = p0.x + (float)((double)(s - tagStart) / total) * width;
            float x1 = p0.x + (float)((double)(e - tagStart) / total) * width;
            if (x1 < x0 + 1.0f) x1 = x0 + 1.0f;

            uint32_t color = info.overlaps
                ? kMinimapOverlapColor
                : minimapColorForType(info.span.type);
            dl->AddRectFilled(ImVec2(x0, p0.y), ImVec2(x1, p1.y), color);
        }

        dl->AddRect(p0, p1, kMinimapBorderColor);

        ImGui::InvisibleButton("##minimap", ImVec2(width, kMinimapHeight));
        if (ImGui::IsItemHovered()) {
            double frac = (ImGui::GetMousePos().x - p0.x) / width;
            if (frac < 0.0) frac = 0.0;
            if (frac > 1.0) frac = 1.0;
            char* addr = tagStart + (ptrdiff_t)(frac * total);

            ImGui::BeginTooltip();
            ImGui::Text("Offset: 0x%zx", (size_t)(addr - tagStart));
            bool any = false;
            for (auto& info : window->minimapOverlaps) {
                if (info.span.start <= addr && addr < info.span.end) {
                    any = true;
                    const char* name = info.span.type ? info.span.type->name : "?";
                    ImGui::TextColored(
                        info.overlaps ? ImColor(kMinimapOverlapColor).Value
                                      : ImColor(minimapColorForType(info.span.type)).Value,
                        "%s [size 0x%zx]%s",
                        name,
                        (size_t)(info.span.end - info.span.start),
                        info.overlaps ? "  (overlap!)" : "");
                }
            }
            if (!any) ImGui::TextDisabled("(no span)");
            ImGui::EndTooltip();
        }
    }

}
