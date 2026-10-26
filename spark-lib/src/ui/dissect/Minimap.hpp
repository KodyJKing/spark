#pragma once

#include "State.hpp"
#include "engine/map/schema/span_map.hpp"
#include "imgui.h"

#include <cmath>
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
    constexpr float kMinimapScrollbarHeight = 8.0f;
    constexpr float kMinimapMinThumbWidth = 12.0f;
    constexpr double kMinimapZoomStep = 1.25;
    constexpr double kMinimapMinVisibleBytes = 16.0;

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

    inline void clampMinimapView(WindowState* window, double maxZoom) {
        if (window->minimapZoom < 1.0) window->minimapZoom = 1.0;
        if (window->minimapZoom > maxZoom) window->minimapZoom = maxZoom;
        double maxStart = 1.0 - 1.0 / window->minimapZoom;
        if (window->minimapViewStart > maxStart) window->minimapViewStart = maxStart;
        if (window->minimapViewStart < 0.0) window->minimapViewStart = 0.0;
    }

    inline void renderMinimap(RenderContext& renderCtx, StructureRef& root) {
        WindowState* window = renderCtx.windowState;

        if (ImGui::SmallButton("Refresh Minimap")) window->minimapBuiltTime = -1.0;
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset Zoom")) {
            window->minimapZoom = 1.0;
            window->minimapViewStart = 0.0;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%.1fx  (wheel: zoom, drag/shift+wheel: pan, double-click: reset)", window->minimapZoom);

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

        // Don't zoom in past the point where kMinimapMinVisibleBytes fill the bar.
        double maxZoom = total / kMinimapMinVisibleBytes;
        if (maxZoom < 1.0) maxZoom = 1.0;
        clampMinimapView(window, maxZoom);

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + width, p0.y + kMinimapHeight);

        // --- Input on the main bar ---
        ImGui::InvisibleButton("##minimap", ImVec2(width, kMinimapHeight),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
        bool barHovered = ImGui::IsItemHovered();
        ImGuiIO& io = ImGui::GetIO();

        if (barHovered) {
            float mouseFrac = (io.MousePos.x - p0.x) / width;
            float wheelY = io.MouseWheel;
            float wheelX = io.MouseWheelH;
            if (io.KeyShift && wheelX == 0.0f) { wheelX = wheelY; wheelY = 0.0f; }

            if (wheelY != 0.0f) {
                // Zoom about the cursor so the byte under it stays put.
                double viewSize = 1.0 / window->minimapZoom;
                double anchor = window->minimapViewStart + mouseFrac * viewSize;
                window->minimapZoom *= std::pow(kMinimapZoomStep, (double)wheelY);
                clampMinimapView(window, maxZoom);
                window->minimapViewStart = anchor - mouseFrac / window->minimapZoom;
                clampMinimapView(window, maxZoom);
            }
            if (wheelX != 0.0f) {
                window->minimapViewStart -= wheelX * 0.1 / window->minimapZoom;
                clampMinimapView(window, maxZoom);
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                window->minimapZoom = 1.0;
                window->minimapViewStart = 0.0;
            }
        }
        if (ImGui::IsItemActive() && io.MouseDelta.x != 0.0f) {
            window->minimapViewStart -= (io.MouseDelta.x / width) / window->minimapZoom;
            clampMinimapView(window, maxZoom);
        }

        double viewSize = 1.0 / window->minimapZoom;
        double viewStartBytes = window->minimapViewStart * total;
        double viewBytes = viewSize * total;
        char* viewStart = tagStart + (ptrdiff_t)viewStartBytes;
        char* viewEnd = tagStart + (ptrdiff_t)std::ceil(viewStartBytes + viewBytes);
        if (viewEnd > tagEnd) viewEnd = tagEnd;

        auto addrToX = [&](char* a) {
            return p0.x + (float)(((double)(a - tagStart) - viewStartBytes) / viewBytes * width);
        };

        // --- Draw main bar ---
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(p0, p1, true);
        dl->AddRectFilled(p0, p1, kMinimapBgColor);

        for (auto& info : window->minimapOverlaps) {
            char* s = info.span.start;
            char* e = info.span.end;
            if (e <= viewStart || s >= viewEnd) continue;
            if (s < tagStart) s = tagStart;
            if (e > tagEnd) e = tagEnd;

            float x0 = addrToX(s);
            float x1 = addrToX(e);
            if (x0 < p0.x - 1.0f) x0 = p0.x - 1.0f;
            if (x1 > p1.x + 1.0f) x1 = p1.x + 1.0f;
            if (x1 < x0 + 1.0f) x1 = x0 + 1.0f;

            uint32_t color = info.overlaps
                ? kMinimapOverlapColor
                : minimapColorForType(info.span.type);
            dl->AddRectFilled(ImVec2(x0, p0.y), ImVec2(x1, p1.y), color);
        }

        dl->PopClipRect();
        dl->AddRect(p0, p1, kMinimapBorderColor);

        if (barHovered && !ImGui::IsItemActive()) {
            double frac = (io.MousePos.x - p0.x) / width;
            if (frac < 0.0) frac = 0.0;
            if (frac > 1.0) frac = 1.0;
            char* addr = tagStart + (ptrdiff_t)(viewStartBytes + frac * viewBytes);
            if (addr >= tagEnd) addr = tagEnd - 1;

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

        // --- Horizontal scrollbar (only meaningful when zoomed in) ---
        if (window->minimapZoom > 1.0) {
            ImVec2 s0 = ImGui::GetCursorScreenPos();
            ImVec2 s1 = ImVec2(s0.x + width, s0.y + kMinimapScrollbarHeight);
            ImGui::InvisibleButton("##minimapScroll", ImVec2(width, kMinimapScrollbarHeight));
            ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

            float thumbW = (float)(viewSize * width);
            if (thumbW < kMinimapMinThumbWidth) thumbW = kMinimapMinThumbWidth;
            if (thumbW > width) thumbW = width;
            float track = width - thumbW;
            double maxStart = 1.0 - viewSize;

            if (ImGui::IsItemActive()) {
                // Center the thumb on the mouse; clicking the track jumps there.
                float mx = io.MousePos.x - s0.x - thumbW * 0.5f;
                window->minimapViewStart = track > 0.0f ? (mx / track) * maxStart : 0.0;
                clampMinimapView(window, maxZoom);
            } else if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f) {
                window->minimapViewStart -= io.MouseWheel * 0.1 * viewSize;
                clampMinimapView(window, maxZoom);
            }

            float thumbX = s0.x + (maxStart > 0.0 ? (float)(window->minimapViewStart / maxStart) * track : 0.0f);
            uint32_t thumbColor = ImGui::GetColorU32(
                ImGui::IsItemActive() ? ImGuiCol_ScrollbarGrabActive
                : ImGui::IsItemHovered() ? ImGuiCol_ScrollbarGrabHovered
                : ImGuiCol_ScrollbarGrab);
            dl->AddRectFilled(s0, s1, ImGui::GetColorU32(ImGuiCol_ScrollbarBg));
            dl->AddRectFilled(ImVec2(thumbX, s0.y), ImVec2(thumbX + thumbW, s1.y), thumbColor, 2.0f);
        }
    }

}