#include "MarioWeaponPose.hpp"
#include "MarioWeaponOffset.hpp"
#include "MarioAimingIK.hpp"
#include "MarioSkeleton.hpp"
#include "MarioShell.hpp"
#include "spark/hook/Hooks.hpp"
#include "engine/halo1.hpp"

namespace Mod::Mario::MarioWeaponPose {

    static void updateWeaponPose(uint32_t weaponHandle) {
        auto rec = Engine::getEntityRecord(weaponHandle);
        if (!rec) return;
        auto weaponEntity = rec->entity();
        if (!weaponEntity) return;

        auto weaponBones = weaponEntity->worldBones.get(weaponEntity, 0);
        if (!weaponBones) return;

        auto leftHandBone = getMarioBoneByName("frame left_hand");

        auto weaponRootBone = &weaponBones[0];
        auto rootBoneInitial = weaponRootBone[0];

        MarioWeaponOffset::Offset off;
        MarioWeaponOffset::getWeaponOffset(weaponHandle, off);
        weaponRootBone->pos = leftHandBone.transformPoint(off);
        if (MarioAimingIK::marioArmsBusy()) {
            weaponRootBone->x = leftHandBone.x;
            // Rotate 180 about x axis to account for difference in hand and weapon coordinate systems.
            weaponRootBone->y = leftHandBone.y * -1.0f;
            weaponRootBone->z = leftHandBone.z * -1.0f;
        }

        auto initialInverse = Engine::inverseTransform(rootBoneInitial);
        auto relativeTransform = Engine::multiplyTransforms(
            *weaponRootBone,
            initialInverse
        );
        auto boneCount = weaponEntity->worldBones.count();
        for (int i = 1; i < boneCount; i++) {
            auto& bone = weaponBones[i];
            bone = Engine::multiplyTransforms(relativeTransform, bone);
        }
    }

    void addHandlers(Spark::ModId modId) {
        Spark::UpdateWorldBones::addHandler(modId, +[](void*, auto next, uint32_t entityHandle) {
            next(entityHandle);

            if (entityHandle == Engine::getPlayerHeldWeaponHandle()) {
                bool wasShell = Shell::updateShellPose(entityHandle);
                if (!wasShell) updateWeaponPose(entityHandle);
            }
        }, nullptr);
    }

}
