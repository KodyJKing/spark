#include "FallDamageFix.hpp"
#include "MarioState.hpp"
#include "spark/hook/Hooks.hpp"
#include <string>
#include "libsm64.h"
#include "decomp/sm64.h"
#include "Coordinates.hpp"

namespace HaloCE::Mod::Mario::FallDamageFix {

    void registerHandlers(Spark::ModId modId) {
        Spark::DamageEntity::addHandler(modId, +[](void* /*ctx*/, auto next, Engine::DamageEvent* event, uint32_t entityHandle, uint16_t p2, uint16_t p3, int16_t hitBoneIndex, uint64_t p5) {
            if (!enableMario || !possessMario) {
                next(event, entityHandle, p2, p3, hitBoneIndex, p5);
                return;
            }

            auto playerHandle = Engine::getPlayerHandle();

            if (entityHandle == playerHandle) {
                auto damageTypeTagId = event->damageTypeTagHandle;
                auto damageTag = Engine::getTag(damageTypeTagId);
                
                // Ignore fall damage to the player when Mario is active.
                std::string path = damageTag ? damageTag->getResourcePath() : "";
                if (path.find("globals\\falling") != std::string::npos) return;
                if (path.find("globals\\distance") != std::string::npos) return;

                // Explosions launch mario.
                if (path.find("explosion") != std::string::npos) {
                    Vec3 impulse = event->hitDirection * event->baseDamage * 0.25;
                    Vec3 marioImpulse = Coordinates::haloToMario(impulse);

                    bool isSelfDamage = (entityHandle == playerHandle);
                    bool wasLongJumping = marioState.action == ACT_LONG_JUMP; 

                    uint32_t action;
                    if (!isSelfDamage) {
                        action = ACT_THROWN_BACKWARD;
                        sm64_set_mario_action(marioId, ACT_THROWN_BACKWARD);
                    } else if (!wasLongJumping) {
                        // sm64_set_mario_action(marioId, ACT_DIVE);
                        // sm64_set_mario_action(marioId, ACT_VERTICAL_WIND);
                        sm64_set_mario_action(marioId, ACT_SHOT_FROM_CANNON);
                    } else {
                        impulse.x *= 10.0f;
                        impulse.y *= 10.0f;
                        impulse.z *= 0.1f;
                    }

                    sm64_set_mario_velocity(marioId, 
                        marioImpulse.x + marioState.velocity[0], 
                        marioImpulse.y + marioState.velocity[1], 
                        marioImpulse.z + marioState.velocity[2]);
                    
                    // Mario doesn't take explosion damage from himself.
                    if (isSelfDamage) {
                        return;
                    }
                }
            }

            // If the damage is dealt by the player and is melee, ignore it.
            auto attackerHandle = event->attackerHandle;
            if (attackerHandle == playerHandle) {
                auto damageTypeTagId = event->damageTypeTagHandle;
                auto damageTag = Engine::getTag(damageTypeTagId);
                std::string path = damageTag ? damageTag->getResourcePath() : "";
                if (path.find("melee") != std::string::npos) return;
            }
            
            
            next(event, entityHandle, p2, p3, hitBoneIndex, p5);

        }, nullptr);
    }

}
