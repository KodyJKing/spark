#include "MarioShell.hpp"

#include "functions/MarioWorldPosition.hpp"
#include "spark/hook/Hooks.hpp"
#include "engine/halo1.hpp"
#include "MarioState.hpp"
#include "MarioSkeleton.hpp"
#include "libsm64.h"
#include "decomp/sm64.h"

namespace Mod::Mario::Shell {

    bool isShield(Engine::Entity* entity) {
        return entity && entity->fromResourcePath(SHIELD_RESOURCE_PATH);
    }
    bool isShield(uint32_t entityHandle) {
        auto entity = Engine::getEntityPointer(entityHandle);
        return isShield(entity);
    }

    bool playerHoldingShield() {
        auto heldWeaponHandle = Engine::getPlayerHeldWeaponHandle();
        auto heldWeapon = Engine::getEntityPointer(heldWeaponHandle);
        return isShield(heldWeapon);
    }

    bool isMarioExitingShell() {
        return marioState.action == ACT_BACKWARD_GROUND_KB || marioState.action == ACT_CROUCH_SLIDE;
    }

    bool isMarioInShellAction() {
        return marioState.action & ACT_FLAG_RIDING_SHELL;
    }

    void putMarioOnShell() {
        if (isMarioExitingShell()) return;
        if (isMarioInShellAction()) return;
        sm64_set_mario_action(marioId, ACT_RIDING_SHELL_GROUND);
    }

    void removeShield() {
        auto inventory = Engine::getPlayerInventory();
        if (inventory) {
            inventory->activeWeaponIndex = 1;
            for (int i = 0; i < 4; i++) {
                auto weaponHandle = inventory->weaponHandles[i];
                if (isShield(inventory->weaponHandles[i])) {
                    inventory->weaponHandles[i] = NULL_HANDLE;
                    auto entity = Engine::getEntityPointer(weaponHandle);
                    if (entity) {
                        entity->parentHandle = NULL_HANDLE;
                    }
                }
            }
        }
    }

    void updateShellState() {
        // if (isMarioExitingShell()) {
        //     removeShield();
        // } else 
        if (playerHoldingShield()) {
            putMarioOnShell();
        }
    }

    bool updateShellPose(uint32_t entityHandle) {
        auto entity = Engine::getEntityPointer(entityHandle);
        if (!isShield(entity)) return false;
        auto pelvis = getMarioBonePointerByName("frame pelvis");
        if (!pelvis) return false;

        auto pos = marioWorldPosition();

        auto bone = entity->worldBones.get(entity, 0);

        float tilt = 0.5f;
        
        Vec3 fwd = (pelvis->x * -1.0f + pelvis->y * tilt).normalize();
        Vec3 up = pelvis->y;
        Vec3 right = up.cross(fwd);
        up = fwd.cross(right);
        
        bone->x = fwd;
        bone->y = right;
        bone->z = up;

        bone->pos = pos + pelvis->x * 0.1f;

        return true;
    }

}
