#include "AdrenalineSystem.hpp"

#include "MarioState.hpp"
#include "engine/halo1.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/RenderBuses.hpp"
#include "decomp/sm64.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include <algorithm>

#define DEBUG_ADRENALINE 1

namespace Mod::Mario::AdrenalineSystem {

    // ── Config ─────────────────────────────────────────────────────────────────

    // Adrenaline decays by this amount per game tick (~30 Hz).
    // At 0.005/tick the meter empties from full in ~6.7 seconds without activity.
    static constexpr float kDecayPerTick         = 0.005f;

    // Boosts awarded on the first frame of each stunt / kill.
    static constexpr float kBoostKill            = 0.40f;
    static constexpr float kBoostWallKick        = 0.30f;
    static constexpr float kBoostGroundPound     = 0.25f;
    static constexpr float kBoostDive            = 0.15f;
    static constexpr float kBoostLongJump        = 0.15f;

    // At or above this level Mario deals full damage.
    // Below it, damage scales linearly down to kMinDamageMultiplier.
    static constexpr float kFullDamageThreshold  = 0.30f;
    static constexpr float kMinDamageMultiplier  = 0.25f;

    // ── State ──────────────────────────────────────────────────────────────────

    static float    sAdrenaline  = 1.0f;     // starts full so combat feels good immediately
    static uint32_t sPrevAction  = UINT32_MAX;

    // ── Helpers ────────────────────────────────────────────────────────────────

    static void addAdrenaline(float amount) {
        sAdrenaline = (std::min)(1.0f, sAdrenaline + amount);
    }

    static float damageMultiplier() {
        if (sAdrenaline >= kFullDamageThreshold)
            return 1.0f;
        float t = sAdrenaline / kFullDamageThreshold;
        return kMinDamageMultiplier + t * (1.0f - kMinDamageMultiplier);
    }

    // ── Handler registration ───────────────────────────────────────────────────

    void registerHandlers(Spark::ModId modId) {

        // ── Per-tick: decay + stunt detection ─────────────────────────────────
        Spark::UpdateAllEntities::addHandler(modId, +[](void*, auto next) {
            next();

            if (!enableMario || !possessMario)
                return;

            sAdrenaline = (std::max)(0.0f, sAdrenaline - kDecayPerTick);

            // Detect the first frame of a stunt action transition.
            uint32_t action = marioState.action;
            if (action != sPrevAction) {
                switch (action) {
                    case ACT_WALL_KICK_AIR:     addAdrenaline(kBoostWallKick);    break;
                    case ACT_GROUND_POUND_LAND: addAdrenaline(kBoostGroundPound); break;
                    case ACT_DIVE:              addAdrenaline(kBoostDive);        break;
                    case ACT_LONG_JUMP:         addAdrenaline(kBoostLongJump);    break;
                    default:                                                       break;
                }
                sPrevAction = action;
            }
        }, nullptr);

        // ── DamageEntity: scale outgoing damage + kill bonus ───────────────────
        Spark::DamageEntity::addHandler(modId, +[](void*, auto next,
            Engine::DamageEvent* event, uint32_t entityHandle,
            uint16_t p2, uint16_t p3, int16_t hitBoneIndex, uint64_t p5)
        {
            if (!enableMario || !possessMario)
                return next(event, entityHandle, p2, p3, hitBoneIndex, p5);

            auto playerHandle = Engine::getPlayerHandle();
            bool isOutgoing   = (event->attackerHandle == playerHandle)
                             && (entityHandle          != playerHandle);

            if (isOutgoing)
                event->damageMultiplier *= damageMultiplier();
                // event->baseDamage *= damageMultiplier();

            auto* victim   = Engine::getEntityPointer(entityHandle);
            bool  wasAlive = victim && victim->health > 0;

            next(event, entityHandle, p2, p3, hitBoneIndex, p5);

            if (isOutgoing && wasAlive && victim->health <= 0)
                addAdrenaline(kBoostKill);

        }, nullptr);
        // ── Debug UI ──────────────────────────────────────────────────────────
        #ifdef DEBUG_ADRENALINE
        {
            using Bus = Spark::EventBus<void>;
            Spark::onRenderDebugUI.addHandler(modId, +[](void*, Bus::Cursor next) {
                next();

                ImGui::Begin("Adrenaline", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

                // Bar colour: green (full) → yellow → red (empty)
                float a = sAdrenaline;
                ImVec4 color = {
                    (std::min)(1.0f, 2.0f * (1.0f - a)),   // R: rises as a falls
                    (std::min)(1.0f, 2.0f * a),             // G: rises as a rises
                    0.0f, 1.0f
                };
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
                ImGui::ProgressBar(a, ImVec2(200.0f, 20.0f));
                ImGui::PopStyleColor();

                ImGui::Text("Adrenaline : %.3f", a);
                ImGui::Text("Dmg mult   : %.2fx", damageMultiplier());

                ImGui::End();
            }, nullptr);
        }
        #endif // DEBUG_ADRENALINE

    }

} // namespace Mod::Mario::AdrenalineSystem
