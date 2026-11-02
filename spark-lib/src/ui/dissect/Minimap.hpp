#pragma once

#include "State.hpp"
#include "engine/map/schema/span_map.hpp"
#include "imgui.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
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
    constexpr float kMinimapProblemStripHeight = 8.0f;
    constexpr float kMinimapProblemTriHalfWidth = 4.0f;
    constexpr uint32_t kMinimapProblemColor = IM_COL32(235, 50, 40, 255);
    constexpr float kMinimapMinStripePixels = 2.0f;
    constexpr float kMinimapStripeDim = 0.7f;

    // Stable color derived from the referenced type's identity, so repeated
    // structures share a hue and layout patterns are easy to spot. Hashes the
    // id rather than the pointer since each analysis uses a fresh schema snapshot.
    inline uint32_t minimapColorForType(StructureNode* type) {
        uint64_t h = type ? (uint64_t)std::hash<std::string>{}(type->id) : 0;
        h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33;
        float hue = (h & 0xffff) / 65535.0f;
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hue, 0.55f, 0.85f, r, g, b);
        return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
    }

    inline uint32_t minimapStripeColor(uint32_t color, bool dim) {
        if (!dim) return color;
        ImVec4 c = ImColor(color).Value;
        return ImColor(c.x * kMinimapStripeDim, c.y * kMinimapStripeDim, c.z * kMinimapStripeDim, c.w);
    }

    // Parity for alternating brightness: a span continues its type's stripe
    // sequence when it starts exactly where the previous span of that type
    // ended. Collapsed spans advance the parity by their element count.
    inline std::vector<uint8_t> computeMinimapStripeParity(const std::vector<Analysis::SpanInfo>& spans) {
        struct Run { char* end; uint32_t nextParity; };
        std::unordered_map<StructureNode*, Run> runs;
        std::vector<uint8_t> parity(spans.size(), 0);
        for (size_t i = 0; i < spans.size(); ++i) {
            const auto& span = spans[i].span;
            uint32_t p = 0;
            auto it = runs.find(span.type);
            if (it != runs.end() && it->second.end == span.start) p = it->second.nextParity;
            parity[i] = (uint8_t)(p & 1);
            size_t elemSize = span.type ? span.type->size : 0;
            size_t elems = elemSize > 0 ? (size_t)(span.end - span.start) / elemSize : 1;
            if (elems == 0) elems = 1;
            runs[span.type] = { span.end, p + (uint32_t)elems };
        }
        return parity;
    }

    inline void clampMinimapView(WindowState* window, double maxZoom) {
        if (window->minimapZoom < 1.0) window->minimapZoom = 1.0;
        if (window->minimapZoom > maxZoom) window->minimapZoom = maxZoom;
        double maxStart = 1.0 - 1.0 / window->minimapZoom;
        if (window->minimapViewStart > maxStart) window->minimapViewStart = maxStart;
        if (window->minimapViewStart < 0.0) window->minimapViewStart = 0.0;
    }

    // Runs the full minimap analysis on a detached thread against a snapshot of
    // the schema. Map memory is read live; WindowState::waitForMinimapJob must
    // be called before the map is freed.
    inline std::shared_ptr<MinimapJob> startMinimapJob(const RenderContext& renderCtx, const StructureRef& root) {
        auto job = std::make_shared<MinimapJob>();
        auto schema = std::make_shared<Schema>(*root.context->schema);
        MapFile* mapFile = root.context->mapFile;
        Id rootId = root.node ? root.node->id : NullId;
        void* rootAddress = root.address;
        char* tagStart = (char*)renderCtx.tagStart;
        char* tagEnd = (char*)renderCtx.tagEnd;
        Analysis::ProblemOptions options = state.problemOptions;

        std::thread([=] {
            auto result = std::make_shared<MinimapAnalysis>();
            result->schema = schema;
            Context ctx = { mapFile, schema.get() };

            auto it = schema->structures.find(rootId);
            if (it != schema->structures.end()) {
                StructureRef snapshotRoot = { &ctx, &it->second, rootAddress };
                Analysis::SpanMap spans;
                // The root struct isn't a block element, so add it explicitly or it
                // would show up as a gap at the start of the tag.
                if (snapshotRoot.node->size > 0) {
                    char* rootStart = (char*)rootAddress;
                    spans.spans.insert({rootStart, rootStart + snapshotRoot.node->size, snapshotRoot.node, Type::Structure});
                }
                Analysis::fillSpanMap(snapshotRoot, spans, tagStart, tagEnd);
                if (!job->cancel) {
                    result->overlaps = Analysis::analyzeOverlaps(spans);
                    result->stripeParity = computeMinimapStripeParity(result->overlaps);
                    result->problems = Analysis::findProblems(result->overlaps, tagStart, tagEnd, options);
                }
                if (!job->cancel) {
                    Hints::findProbableTypeProblems(&ctx, result->overlaps, tagStart, tagEnd, result->problems);
                }
            }

            job->result = std::move(result);
            job->done = true;
        }).detach();

        return job;
    }

    inline void renderMinimap(RenderContext& renderCtx, StructureRef& root) {
        WindowState* window = renderCtx.windowState;

        if (ImGui::SmallButton("Refresh Minimap")) window->minimapRefreshRequested = true;
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset Zoom")) {
            window->minimapZoom = 1.0;
            window->minimapViewStart = 0.0;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Ignore gaps after DataPointers", &state.problemOptions.ignoreGapsAfterDataPointer))
            window->minimapRefreshRequested = true;
        ImGui::SetItemTooltip("Opaque data (DataPointer payloads) can be followed by padding for 4-byte alignment.");
        ImGui::SameLine();
        ImGui::TextDisabled("%.1fx  (wheel: zoom, drag/shift+wheel: pan, double-click: reset)", window->minimapZoom);

        if (window->minimapJob && window->minimapJob->done) {
            window->minimap = std::move(window->minimapJob->result);
            window->minimapJob.reset();
            window->minimapBuiltTime = ImGui::GetTime();
        }

        // Throttle from the last completed build so a slow analysis doesn't
        // keep a core busy back-to-back.
        double now = ImGui::GetTime();
        bool stale = window->minimapRefreshRequested ||
            window->minimapBuiltTime < 0.0 ||
            now - window->minimapBuiltTime >= kMinimapRebuildSeconds;
        if (stale && !window->minimapJob) {
            window->minimapRefreshRequested = false;
            window->minimapJob = startMinimapJob(renderCtx, root);
        }

        if (!window->minimap) {
            ImGui::TextDisabled("[minimap: analyzing...]");
            return;
        }
        // Hold a reference so the snapshot stays alive for this frame.
        std::shared_ptr<MinimapAnalysis> analysis = window->minimap;
        const auto& overlaps = analysis->overlaps;
        const auto& problems = analysis->problems;
        const auto& stripeParity = analysis->stripeParity;

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
        ImVec2 strip0 = ImVec2(p0.x, p1.y);
        ImVec2 strip1 = ImVec2(p1.x, p1.y + kMinimapProblemStripHeight);

        // --- Input on the main bar (and the problem strip beneath it) ---
        ImGui::InvisibleButton("##minimap", ImVec2(width, kMinimapHeight + kMinimapProblemStripHeight),
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

        for (size_t spanIndex = 0; spanIndex < overlaps.size(); ++spanIndex) {
            auto& info = overlaps[spanIndex];
            char* s = info.span.start;
            char* e = info.span.end;
            if (e <= viewStart || s >= viewEnd) continue;
            if (s < tagStart) s = tagStart;
            if (e > tagEnd) e = tagEnd;

            uint32_t baseColor = info.overlaps
                ? kMinimapOverlapColor
                : minimapColorForType(info.span.type);
            uint32_t parity = spanIndex < stripeParity.size()
                ? stripeParity[spanIndex] : 0;

            // Collapsed multi-element spans (huge blocks) get striped per element
            // once elements are wide enough on screen to tell apart.
            StructureNode* type = info.span.type;
            size_t elemSize = type ? type->size : 0;
            if (!info.overlaps && elemSize > 0 &&
                (size_t)(info.span.end - info.span.start) > elemSize &&
                (double)elemSize / viewBytes * width >= kMinimapMinStripePixels) {
                size_t firstElem = s > viewStart ? 0 : (size_t)(viewStart - info.span.start) / elemSize;
                for (size_t i = firstElem;; ++i) {
                    char* es = info.span.start + i * elemSize;
                    if (es >= e || es >= viewEnd) break;
                    char* ee = es + elemSize;
                    if (ee > e) ee = e;
                    float x0 = addrToX(es < s ? s : es);
                    float x1 = addrToX(ee);
                    if (x0 < p0.x - 1.0f) x0 = p0.x - 1.0f;
                    if (x1 > p1.x + 1.0f) x1 = p1.x + 1.0f;
                    uint32_t color = minimapStripeColor(baseColor, (parity + (uint32_t)i) & 1);
                    dl->AddRectFilled(ImVec2(x0, p0.y), ImVec2(x1, p1.y), color);
                }
                continue;
            }

            float x0 = addrToX(s);
            float x1 = addrToX(e);
            if (x0 < p0.x - 1.0f) x0 = p0.x - 1.0f;
            if (x1 > p1.x + 1.0f) x1 = p1.x + 1.0f;
            if (x1 < x0 + 1.0f) x1 = x0 + 1.0f;

            uint32_t color = info.overlaps ? baseColor : minimapStripeColor(baseColor, parity & 1);
            dl->AddRectFilled(ImVec2(x0, p0.y), ImVec2(x1, p1.y), color);
        }

        dl->PopClipRect();
        dl->AddRect(p0, p1, kMinimapBorderColor);

        // --- Problem markers: an underline across the range plus a triangle
        // pointing up at its center, so even 1-byte problems stay visible. ---
        auto problemMarkerX = [&](const Analysis::Problem& p) {
            float x0 = addrToX(p.start);
            float x1 = addrToX(p.end);
            float cx = (x0 + x1) * 0.5f;
            // Keep the marker on-screen when the range is only partially visible.
            if (cx < p0.x && x1 > p0.x) cx = (p0.x + (x1 < p1.x ? x1 : p1.x)) * 0.5f;
            if (cx > p1.x && x0 < p1.x) cx = ((x0 > p0.x ? x0 : p0.x) + p1.x) * 0.5f;
            return cx;
        };

        dl->PushClipRect(ImVec2(strip0.x - kMinimapProblemTriHalfWidth, strip0.y), 
                         ImVec2(strip1.x + kMinimapProblemTriHalfWidth, strip1.y), true);
        for (auto& problem : problems) {
            if (problem.end <= viewStart || problem.start >= viewEnd) continue;
            float x0 = addrToX(problem.start);
            float x1 = addrToX(problem.end);
            if (x0 < p0.x) x0 = p0.x;
            if (x1 > p1.x) x1 = p1.x;
            if (x1 - x0 >= 2.0f)
                dl->AddRectFilled(ImVec2(x0, strip0.y), ImVec2(x1, strip0.y + 2.0f), kMinimapProblemColor);

            float cx = problemMarkerX(problem);
            dl->AddTriangleFilled(
                ImVec2(cx, strip0.y + 1.0f),
                ImVec2(cx - kMinimapProblemTriHalfWidth, strip1.y),
                ImVec2(cx + kMinimapProblemTriHalfWidth, strip1.y),
                kMinimapProblemColor);
        }
        dl->PopClipRect();

        if (barHovered && !ImGui::IsItemActive()) {
            double frac = (io.MousePos.x - p0.x) / width;
            if (frac < 0.0) frac = 0.0;
            if (frac > 1.0) frac = 1.0;
            char* addr = tagStart + (ptrdiff_t)(viewStartBytes + frac * viewBytes);
            if (addr >= tagEnd) addr = tagEnd - 1;

            ImGui::BeginTooltip();
            ImGui::Text("Offset: 0x%zx", (size_t)(addr - tagStart));
            bool any = false;
            for (auto& info : overlaps) {
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

            // Problems at the hovered byte, or whose triangle is under the mouse.
            bool anyProblem = false;
            for (auto& problem : problems) {
                if (problem.end <= viewStart || problem.start >= viewEnd) continue;
                bool contains = problem.start <= addr && addr < problem.end;
                bool nearMarker = std::fabs(io.MousePos.x - problemMarkerX(problem)) <= kMinimapProblemTriHalfWidth;
                if (!contains && !nearMarker) continue;
                if (!anyProblem) ImGui::Separator();
                anyProblem = true;
                ImGui::TextColored(ImColor(kMinimapProblemColor).Value,
                    "%s: 0x%zx - 0x%zx [size 0x%zx]",
                    Analysis::problemKindName(problem.kind),
                    (size_t)(problem.start - tagStart),
                    (size_t)(problem.end - tagStart),
                    (size_t)(problem.end - problem.start));
            }
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