#include "MarioMelee.hpp"

#include "MarioState.hpp"
#include "MarioSkeleton.hpp"
#include "MarioModel.hpp"
#include "engine/halo1.hpp"
#include "engine/entity/entity_list.hpp"
#include "spark/hook/Hooks.hpp"

#include "decomp/sm64.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "spark/overlay/ESP.hpp"

#include <cmath>
#include <algorithm>

// #define DEBUG_MARIO_MELEE 1

namespace HaloCE::Mod::Mario::MarioMelee {

    // ── Tunable constants ──────────────────────────────────────────────────────
    static constexpr float kFistRadius              = 0.05f;  // world units
    static constexpr float kFistOffset              = 0.05f;  // distance from hand bone to center of fist hit sphere
    static constexpr float kFootRadius              = 0.06f;  // world units
    static constexpr float kFootOffset              = 0.05f;  // distance from foot bone to center of foot hit sphere
    static constexpr float kBipedCapsuleRadius      = 0.15f;  // world units
    static constexpr float kBipedCapsuleHalfHeight  = 0.25f;  // half-height of cylinder segment
    static constexpr float kVehicleCapsuleRadius    = 0.80f;
    static constexpr float kVehicleCapsuleHalfHeight = 0.50f;
    static constexpr float kMeleeDamage             = 0.01f;
    static constexpr int   kCooldownTicks           = 15;     // ~0.5 s at 30 fps, per fist
    static constexpr float kDebugRange              = 10.0f;  // world units
    
    static const char * kDamageTagPath = "weapons\\frag grenade\\explosion";

    // ── State ──────────────────────────────────────────────────────────────────
    // Index 0 = left fist, 1 = right fist, 2 = left foot, 3 = right foot
    static uint32_t sCooldown[4] = { 0, 0, 0, 0 };

    // ── Helpers ────────────────────────────────────────────────────────────────
    struct CapsuleParams { float radius, halfHeight; };

    static bool isMeleeAction() {
        return (marioState.action & ACT_FLAG_ATTACKING) != 0;
    }

    static Engine::Tag* getDamageTag() {
        return Engine::findTag(kDamageTagPath, "jpt!");
    }

    static Vec3 fistPosition(size_t fistIndex) {
        auto boneName = (fistIndex == 0) ? "left_hand" : "right_hand";
        Engine::WorldTransform bone = getMarioBoneByName(boneName);
        Vec3 pos = bone.pos;
        Vec3 forward = bone.x;
        return pos + forward * kFistOffset;
    }

    static Vec3 footPosition(size_t footIndex) {
        auto boneName = (footIndex == 0) ? "left_foot" : "right_foot";
        Engine::WorldTransform bone = getMarioBoneByName(boneName);
        Vec3 pos = bone.pos;
        Vec3 forward = bone.x;
        return pos + forward * kFootOffset;
    }

    static CapsuleParams capsuleParams(Engine::Entity* e) {
        if (e->entityCategory == Engine::EntityCategory_Vehicle)
            return { kVehicleCapsuleRadius, kVehicleCapsuleHalfHeight };
        return { kBipedCapsuleRadius, kBipedCapsuleHalfHeight };
    }

    // Closest point on segment [A,B] to point P.
    static Vec3 closestOnSegment(Vec3 A, Vec3 B, Vec3 P) {
        Vec3  ab = B - A;
        float d2 = ab.dot(ab);
        if (d2 < 1e-8f) return A;
        float t  = std::clamp((P - A).dot(ab) / d2, 0.f, 1.f);
        return A + ab * t;
    }

    static bool fistHitsCapsule(Vec3 fistPos, Vec3 capA, Vec3 capB, float capsuleR, float sphereR, Vec3& outClosestPoint, Vec3& outHitNormal) {
        Vec3  q    = closestOnSegment(capA, capB, fistPos);
        Vec3  diff = fistPos - q;
        float r    = sphereR + capsuleR;
        if (diff.dot(diff) < r * r) {
            outClosestPoint = q;
            outHitNormal = diff.normalize();
            return true;
        }
        return false;
    }

    static void dealDamage(uint32_t targetHandle, Vec3 hitPos, Vec3 hitDir = { 0.f, 0.f, -1.f }) {
        if (!Spark::DamageEntity::original) return;
        auto* tag = getDamageTag();
        if (!tag) return;

        Engine::DamageEvent ev{};
        ev.damageTypeTagHandle = tag->tagID;
        ev.flags               = 0x1;           // single-entity target
        ev.interactorHandle    = NULL_HANDLE;
        ev.attackerHandle      = Engine::getPlayerHandle();
        ev.sourceTypeIndex     = (uint16_t)-1;
        ev.hitPosition         = hitPos;
        ev.hitDirection        = hitDir;
        ev.baseDamage          = kMeleeDamage;
        ev.damageMultiplier    = 1.0f;
        Spark::DamageEntity::original(&ev, targetHandle, 0, 0, -1, 0);
    }

    // ── Debug draw helpers ─────────────────────────────────────────────────────
    // Draw a world-space ring (circle in the plane perpendicular to `axisN`).
    static void drawRing3D(Vec3 center, Vec3 axisN, float r, int segs, ImU32 col) {
        Vec3 tmp = (fabsf(axisN.x) < 0.9f) ? Vec3{ 1, 0, 0 } : Vec3{ 0, 1, 0 };
        Vec3 u   = axisN.cross(tmp).normalize();
        Vec3 v   = axisN.cross(u).normalize();

        constexpr float k2Pi = 6.28318530f;
        Vec3 prev = center + u * r;
        for (int i = 1; i <= segs; i++) {
            float a    = k2Pi * i / (float)segs;
            Vec3  curr = center + (u * cosf(a) + v * sinf(a)) * r;
            Spark::Overlay::ESP::drawLine(prev, curr, col);
            prev = curr;
        }
    }

    static void drawCapsule3D(Vec3 pos, Vec3 up, float r, float hh, ImU32 col) {
        Vec3 upN = up.normalize();
        Vec3 a   = pos - upN * hh;
        Vec3 b   = pos + upN * hh;

        drawRing3D(a, upN, r, 12, col);
        drawRing3D(b, upN, r, 12, col);

        // 4 connecting staves at 0°, 90°, 180°, 270°
        Vec3 tmp = (fabsf(upN.x) < 0.9f) ? Vec3{ 1, 0, 0 } : Vec3{ 0, 1, 0 };
        Vec3 u   = upN.cross(tmp).normalize();
        Vec3 v   = upN.cross(u).normalize();
        for (int i = 0; i < 4; i++) {
            float angle  = 1.5707963f * i;   // π/2 · i
            Vec3  offset = (u * cosf(angle) + v * sinf(angle)) * r;
            Spark::Overlay::ESP::drawLine(a + offset, b + offset, col);
        }
    }

    // ── Public API ─────────────────────────────────────────────────────────────
    void tick() {
        if (!enableMario || !possessMario) return;

        // Prefetch damage tag
        getDamageTag();
        
        for (int i = 0; i < 4; i++) if (sCooldown[i] > 0) --sCooldown[i];

        // Skip entity iteration if all hit spheres are still cooling down.
        if (sCooldown[0] > 0 && sCooldown[1] > 0 && sCooldown[2] > 0 && sCooldown[3] > 0) return;

        if (!isMeleeAction()) return;

        struct HitSphere { Vec3 pos; float radius; };
        HitSphere spheres[4] = {
            { fistPosition(0), kFistRadius },
            { fistPosition(1), kFistRadius },
            { footPosition(0), kFootRadius },
            { footPosition(1), kFootRadius },
        };

        auto playerHandle = Engine::getPlayerHandle();
        auto marioHandle  = MarioModel::marioHandle;

        Engine::foreachEntityRecordIndexed([&](Engine::EntityRecord* rec, uint16_t idx) {
            auto* entity = rec->entity();
            if (!entity) return;

            auto cat = entity->entityCategory;
            if (cat != Engine::EntityCategory_Biped && cat != Engine::EntityCategory_Vehicle)
                return;

            uint32_t handle = (uint32_t)rec->id << 16 | idx;
            if (handle == playerHandle || handle == marioHandle) return;

            auto [capsR, capsHH] = capsuleParams(entity);
            Vec3 upN  = entity->up.normalize();
            // Bipeds: entity->pos is foot position — offset center up so bottom cap sits at foot level.
            Vec3 capsuleCenter = (cat == Engine::EntityCategory_Biped)
                ? entity->pos + upN * (capsHH + capsR)
                : entity->pos;
            Vec3 capA = capsuleCenter - upN * capsHH;
            Vec3 capB = capsuleCenter + upN * capsHH;

            Vec3 hitNormal; // out parameter for fistHitsCapsule
            Vec3 closestPoint; // out parameter for fistHitsCapsule

            for (int f = 0; f < 4; f++) {
                if (sCooldown[f] > 0) continue;
                if (fistHitsCapsule(spheres[f].pos, capA, capB, capsR, spheres[f].radius, closestPoint, hitNormal)) {
                    dealDamage(handle, closestPoint, hitNormal * -1.f);
                    sCooldown[f] = kCooldownTicks;
                }
            }
        });
    }

    void debugRender() {
        #ifdef DEBUG_MARIO_MELEE
        if (!enableMario || !possessMario) return;

        namespace ESP = Spark::Overlay::ESP;

        struct { Vec3 pos; float radius; } spheres[4] = {
            { fistPosition(0), kFistRadius },
            { fistPosition(1), kFistRadius },
            { footPosition(0), kFootRadius },
            { footPosition(1), kFootRadius },
        };

        // Hit spheres — billboard circles. Red when ready, black otherwise.
        bool canMelee = isMeleeAction();
        constexpr ImU32 kReady = IM_COL32(255, 30,  30,  220);
        constexpr ImU32 kIdle  = IM_COL32(0,   0,   0,   220);
        for (int i = 0; i < 4; i++)
            ESP::drawCircle(spheres[i].pos, spheres[i].radius, (canMelee && sCooldown[i] == 0) ? kReady : kIdle, true);

        // Use pelvis as the distance reference for nearby entity culling.
        Vec3 marioPos = getMarioBoneByName("pelvis").pos;

        Engine::foreachEntityRecord([&](Engine::EntityRecord* rec) {
            auto* entity = rec->entity();
            if (!entity) return;

            auto cat = entity->entityCategory;
            if (cat != Engine::EntityCategory_Biped && cat != Engine::EntityCategory_Vehicle)
                return;

            Vec3 diff = entity->pos - marioPos;
            if (diff.dot(diff) > kDebugRange * kDebugRange) return;

            auto [capsR, capsHH] = capsuleParams(entity);
            Vec3 upN = entity->up.normalize();
            Vec3 capsuleCenter = (cat == Engine::EntityCategory_Biped)
                ? entity->pos + upN * (capsHH + capsR)
                : entity->pos;
            ImU32 col = (cat == Engine::EntityCategory_Vehicle)
                ? IM_COL32(0, 180, 255, 200)
                : IM_COL32(60, 255, 60,  200);
            drawCapsule3D(capsuleCenter, entity->up, capsR, capsHH, col);
        });
        #endif
    }

}
