#include "MarioAimingIK.hpp"

#include "Mario.hpp"
#include "MarioSkeleton.hpp"
#include "MarioInverseKinematics.hpp"
#include "MarioState.hpp"
#include "engine/halo1.hpp"
#include "decomp/sm64.h"

namespace Mod::Mario::MarioAimingIK {

    static bool marioRightArmBusy() {
        if (marioState.action == ACT_IDLE) return false;
        return true;
    }

    static Vec3 preferredArmDirection() {
        auto chest = getMarioBoneByName("frame chest");
        return (chest.y - chest.z * 0.7f).normalize();
    }

    bool marioArmsBusy() {
        auto camera = Engine::getPlayerCameraPointer();
        if (camera) {
            auto marioForward = preferredArmDirection();
            float alignment = marioForward.dot(camera->fwd);
            if (alignment < 0) return true;
        }

        if (marioState.action == ACT_PUNCHING) return true;

        return false; // ???
        if (marioState.action == ACT_IDLE) return false;
        if (marioState.action == ACT_WALKING) return false;
        if (marioState.action == ACT_RIDING_SHELL_GROUND) return false;

        return true;
    }

    static void ikToWeapon() {
        if (marioArmsBusy()) return;

        auto weaponHandle = Engine::getPlayerHeldWeaponHandle();
        if (weaponHandle == 0xFFFFFFFF) return;
        auto rec = Engine::getEntityRecord(weaponHandle);
        if (!rec) return;
        auto weapon = rec->entity();
        if (!weapon) return;
        auto weaponRootBone = weapon->worldBones.get(weapon, 0);
        if (!weaponRootBone) return;

        auto camera = Engine::getPlayerCameraPointer();
        if (!camera) return;

        auto chest    = getMarioBoneByName("frame chest");
        auto rightArm = getMarioBoneByName("frame right_arm");
        
        auto fwd   = camera->fwd;
        auto right = weaponRootBone->y;

        auto base    = rightArm.pos;
        Vec3 coneDir = preferredArmDirection();
        auto dir     = (fwd - right * 0.5f).normalize().projectToCone(coneDir, 0.6f);
        auto target  = base + dir;

        InverseKinematics::MarioIKRequest ikRequest;
        ikRequest.limb             = InverseKinematics::MarioIKRequest::Limb::LeftArm;
        ikRequest.targetTransform  = *weaponRootBone;

        // Rotate target transform 180 about it's x axis to account for the difference in coordinate systems.
        ikRequest.targetTransform.y *= -1.0f;
        ikRequest.targetTransform.z *= -1.0f;

        ikRequest.targetTransform.pos = target;
        ikRequest.enforceRotation  = true;
        InverseKinematics::applyMarioIK(ikRequest);

        // if (marioRightArmBusy()) return;
        // ikRequest.limb = InverseKinematics::MarioIKRequest::Limb::RightArm;
        // ikRequest.targetTransform.pos = target + right * 0.05f;
        // InverseKinematics::applyMarioIK(ikRequest);
    }

    void apply() {
        ikToWeapon();
    }

}
