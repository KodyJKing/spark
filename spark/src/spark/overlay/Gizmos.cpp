#include "Gizmos.hpp"

#include "spark/Spark.hpp"
#include "ESP.hpp"
#include "math/Math.hpp"

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

namespace Spark::Overlay::Gizmos {

    enum class GizmoType { Line, Point, Text };

    struct Gizmo {
        GizmoType type;
        Vec3 a;
        Vec3 b;
        std::string text;
        ImU32 color;
        uint32_t framesRemaining;
        uint32_t totalFrames;
    };

    // Fraction of a gizmo's lifetime (measured from the end) over which its
    // alpha fades from full to zero.
    static constexpr float kFadeFraction = 0.5f;

    // Producers (game/update threads) push gizmos; the render thread drains them
    // in render(). The mutex keeps the queue safe across that thread boundary.
    static std::mutex s_mutex;
    static std::vector<Gizmo> s_gizmos;

    // Scale a packed ImU32 color's alpha channel by factor (0..1).
    static ImU32 scaleAlpha(ImU32 color, float factor) {
        ImU32 alpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
        alpha = (ImU32)(alpha * factor);
        return (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
    }

    void drawLine(Vec3& start, Vec3& end, ImU32 color, uint32_t durationFrames) {
        if (!Spark::showDebugOverlay) return;
        std::lock_guard<std::mutex> lock(s_mutex);
        s_gizmos.push_back(Gizmo{ GizmoType::Line, start, end, std::string{}, color, durationFrames, durationFrames });
    }

    void drawPoint(Vec3& position, ImU32 color, uint32_t durationFrames) {
        if (!Spark::showDebugOverlay) return;
        std::lock_guard<std::mutex> lock(s_mutex);
        s_gizmos.push_back(Gizmo{ GizmoType::Point, position, Vec3{}, std::string{}, color, durationFrames, durationFrames });
    }

    void drawText(Vec3& center, const char* text, ImU32 color, uint32_t durationFrames) {
        if (!Spark::showDebugOverlay) return;
        std::lock_guard<std::mutex> lock(s_mutex);
        s_gizmos.push_back(Gizmo{ GizmoType::Text, center, Vec3{}, std::string{text ? text : ""}, color, durationFrames, durationFrames });
    }

    void render() {
        std::lock_guard<std::mutex> lock(s_mutex);

        for (auto& g : s_gizmos) {
            // life goes 1.0 at spawn -> 0.0 at death; fade alpha over the final
            // kFadeFraction of the lifetime.
            float life = g.totalFrames > 0 ? (float)g.framesRemaining / (float)g.totalFrames : 1.0f;
            float fade = Math::smoothstep(0.0f, kFadeFraction, life);
            ImU32 color = scaleAlpha(g.color, fade);

            switch (g.type) {
                case GizmoType::Line:  ESP::drawLine(g.a, g.b, color); break;
                case GizmoType::Point: ESP::drawPoint(g.a, color); break;
                case GizmoType::Text:  ESP::drawText(g.a, g.text.c_str(), color); break;
            }
            if (g.framesRemaining > 0)
                g.framesRemaining -= 1;
        }

        s_gizmos.erase(
            std::remove_if(s_gizmos.begin(), s_gizmos.end(),
                [](const Gizmo& g) { return g.framesRemaining == 0; }),
            s_gizmos.end()
        );
    }

}