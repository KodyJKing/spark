#include "MarioMelee.hpp"

#include "MarioState.hpp"
#include "MarioSkeleton.hpp"
#include "MarioModel.hpp"
#include "MarioShieldRegen.hpp"
#include "engine/halo1.hpp"
#include "engine/entity/entity_list.hpp"
#include "engine/raycast.hpp"
#include "spark/hook/Hooks.hpp"

#include "decomp/sm64.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "spark/overlay/ESP.hpp"

#include <cmath>
#include <algorithm>

// #define DEBUG_MARIO_MELEE 1

#ifdef DEBUG_MARIO_MELEE
    #include <iostream>
    #define LOG(x) std::cout << "[MarioMelee] " << x << std::endl;
#else
    #define LOG(x) ;
#endif

namespace Mod::Mario::MarioMelee {

    // ── Tunable constants ──────────────────────────────────────────────────────
    static constexpr float kFistRadius              = 0.05f;  // world units (debug marker only)
    static constexpr float kFistOffset              = 0.05f;  // distance from hand bone to fist hit point
    static constexpr float kFootRadius              = 0.06f;  // world units (debug marker only)
    static constexpr float kFootOffset              = 0.05f;  // distance from foot bone to foot hit point
    static constexpr float kMeleeDamage             = 0.01f;
    static constexpr int   kCooldownTicks           = 15;     // ~0.5 s at 30 fps, per limb
    static constexpr float kMeleeShieldRegen        = 0.2f;   // shield regen per melee hit
    static constexpr float kLimbSweepMultiplier     = 1.5f;   // multiplier for limb sweep distance
    
    static const char * kDamageTagPath = "weapons\\frag grenade\\explosion";
    static const char * kBipedImpactSoundTagPath = "sound\\sfx\\impulse\\melee\\melee_impact_fleshy";

    // ── State ──────────────────────────────────────────────────────────────────
    // Index 0 = left fist, 1 = right fist, 2 = left foot, 3 = right foot
    static uint32_t sCooldown[4] = { 0, 0, 0, 0 };
    // Previous-frame limb positions, used as the origin of each swept raycast.
    static Vec3 sPrevPos[4] = {};
    static bool sPrevValid = false;

    // ── Helpers ────────────────────────────────────────────────────────────────
    static bool isMeleeAction() {
        return (marioState.action & ACT_FLAG_ATTACKING) != 0;
    }

    static Engine::Tag* getDamageTag() {
        return Engine::findTag(kDamageTagPath, "jpt!");
    }

    static Engine::Tag* getBipedImpactSoundTag() {
        return Engine::findTag(kBipedImpactSoundTagPath, "snd!");
    }

    static Vec3 fistPosition(size_t fistIndex) {
        auto boneName = (fistIndex == 0) ? "left_hand" : "right_hand";
        Engine::Transform bone = getMarioBoneByName(boneName);
        Vec3 pos = bone.pos;
        Vec3 forward = bone.x;
        return pos + forward * kFistOffset;
    }

    static Vec3 footPosition(size_t footIndex) {
        auto boneName = (footIndex == 0) ? "left_foot" : "right_foot";
        Engine::Transform bone = getMarioBoneByName(boneName);
        Vec3 pos = bone.pos;
        Vec3 forward = bone.x;
        return pos + forward * kFootOffset;
    }

    static void dealDamage(uint32_t targetHandle, Vec3 hitPos, Vec3 hitDir = { 0.f, 0.f, -1.f }, int16_t boneIndex = 0) {
        if (!Spark::DamageEntity::original) return;
        auto* tag = getDamageTag();
        if (!tag) return;

        auto targetEntity = Engine::getEntityPointer(targetHandle);
        bool targetWasAlive = targetEntity && targetEntity->health > 0; 

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
        Spark::DamageEntity::original(&ev, targetHandle, 0, 0, boneIndex, 0);

        auto* soundTag = getBipedImpactSoundTag();
        if (soundTag) {
            Spark::SoundImpulseStart::original(soundTag->tagID, 0xFFFFFFFF, 1);
        }

        if (targetWasAlive) {
            auto player = Engine::getPlayerEntity();
            if (player) {
                regenerateShield(*player, kMeleeShieldRegen, false);
            }
        }
    }

    // ── Public API ─────────────────────────────────────────────────────────────
    void tick() {
        if (!enableMario || !possessMario) {
            sPrevValid = false;
            return;
        }

        // Prefetch tags to avoid hitches during gameplay.
        getDamageTag();
        getBipedImpactSoundTag();

        for (int i = 0; i < 4; i++) if (sCooldown[i] > 0) --sCooldown[i];

        Vec3 currPos[4] = {
            fistPosition(0),
            fistPosition(1),
            footPosition(0),
            footPosition(1),
        };

        // On the first valid frame we have no previous position to sweep from.
        if (!sPrevValid) {
            for (int i = 0; i < 4; i++) sPrevPos[i] = currPos[i];
            sPrevValid = true;
            return;
        }

        if (isMeleeAction()) {
            auto playerHandle = Engine::getPlayerHandle();
            auto marioHandle  = MarioModel::marioHandle;
            uint32_t sourceHandle = (marioHandle != NULL_HANDLE) ? marioHandle : playerHandle;

            for (int f = 0; f < 4; f++) {
                if (sCooldown[f] > 0) continue;

                Vec3 origin       = sPrevPos[f];
                Vec3 displacement = (currPos[f] - origin) * kLimbSweepMultiplier;
                if (displacement.dot(displacement) < 1e-8f) continue; // limb didn't move

                Engine::RaycastResult result{};
                Engine::raycast(ENGINE_RAYCAST_PROJECTILE_FLAGS, &origin, &displacement, sourceHandle, &result);

                if (result.hitType != Engine::HitType_Entity) continue;

                uint32_t handle = result.entityHandle;
                if (handle == playerHandle || handle == marioHandle) continue;

                auto* hitEntity = Engine::getEntityPointer(handle);
                if (!hitEntity) continue;
                auto cat = hitEntity->entityCategory;
                if (cat != Engine::EntityCategory_Biped && cat != Engine::EntityCategory_Vehicle) continue;

                // Log the bone index
                LOG("Bone index: " << result.boneIndex);

                dealDamage(handle, result.pos, result.normal.normalize() * -1.0f);
                sCooldown[f] = kCooldownTicks;
            }
        }

        for (int i = 0; i < 4; i++) sPrevPos[i] = currPos[i];
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

        // Hit markers — billboard circles. Red when ready, black otherwise.
        bool canMelee = isMeleeAction();
        constexpr ImU32 kReady = IM_COL32(255, 30,  30,  220);
        constexpr ImU32 kIdle  = IM_COL32(0,   0,   0,   220);
        for (int i = 0; i < 4; i++)
            ESP::drawCircle(spheres[i].pos, spheres[i].radius, (canMelee && sCooldown[i] == 0) ? kReady : kIdle, true);

        // Swept raycast segments from previous to current limb position.
        if (sPrevValid) {
            for (int i = 0; i < 4; i++) {
                ImU32 col = (canMelee && sCooldown[i] == 0)
                    ? IM_COL32(255, 30, 30, 200)
                    : IM_COL32(120, 120, 120, 160);
                ESP::drawLine(sPrevPos[i], spheres[i].pos, col);
            }
        }
        #endif
    }

}
