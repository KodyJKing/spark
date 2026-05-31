#include "Mario.hpp"
#include "MarioModel.hpp"
#include "MarioSkeleton.hpp" 
#include "MarioInverseKinematics.hpp"
#include "MarioWeaponOffset.hpp"
#include <string>
#include "decomp/sm64.h"
#include "Coordinates.hpp"
namespace HaloCE::Mod::Mario::MarioModel {
    const char* marioTagPath = "smc64\\mario\\mario";

    bool isMario(Engine::Entity* entity) {
        if (!entity) return false;
        return entity->fromResourcePath(marioTagPath);
    }

    bool isMario(uint32_t entityHandle) {
        auto rec = Engine::getEntityRecord( entityHandle );
        if (!rec) return false;
        auto entity = rec->entity();
        if (!entity) return false;
        return isMario(entity);
    }

    uint32_t playerWeaponHandle() {
        auto playerEntity = Engine::getPlayerEntity();
        if (!playerEntity) return 0xFFFFFFFF;
        return playerEntity->childHandle;
    }

    bool marioRightArmBusy() {
        if (marioState.action == ACT_IDLE) return false;
        return true;
    }

    Vec3 preferredArmDirection() {
        auto chest = getMarioBoneByName("chest");
        return (chest.z + chest.y * 0.7f).normalize() * -1.0f;
    }

    bool marioArmsBusy() {
        auto camera = Engine::getPlayerCameraPointer();
        if (camera) {
            auto marioForward = preferredArmDirection();
            float alignment = marioForward.dot(camera->fwd);
            if (alignment < 0) return true;
        }

        // If punching, don't IK to weapon.
        if (marioState.action == ACT_PUNCHING) return true;

        return false; // ???
        if (marioState.action == ACT_IDLE) return false;
        if (marioState.action == ACT_WALKING) return false;
        if (marioState.action == ACT_RIDING_SHELL_GROUND) return false;

        return true;
    }

    void IKToWeapon() {
        // If Mario is not idle, skip this.
        if (marioArmsBusy()) return;

        auto weaponHandle = playerWeaponHandle();
        if (weaponHandle == 0xFFFFFFFF) return;
        auto rec = Engine::getEntityRecord( weaponHandle );
        if (!rec) return;
        auto weapon = rec->entity();
        if (!weapon) return;
        auto weaponRootBone = weapon->worldBones.get(weapon, 0);
        if (!weaponRootBone) return;
        
        auto camera = Engine::getPlayerCameraPointer();
        if (!camera) return;

        auto chest = getMarioBoneByName("chest");
        auto rightArm = getMarioBoneByName("right_arm");

        auto fwd = camera->fwd;
        auto right = weaponRootBone->y;

        auto base = rightArm.pos;
        Vec3 coneDir = preferredArmDirection();
        auto dir = (fwd - right * 0.5f).normalize().projectToCone(coneDir, 0.6f);
        auto target = base + dir;

        InverseKinematics::MarioIKRequest ikRequest;
        ikRequest.limb = InverseKinematics::MarioIKRequest::Limb::LeftArm;
        ikRequest.targetTransform = *weaponRootBone;
        ikRequest.targetTransform.pos = target;
        ikRequest.enforceRotation = true;
        InverseKinematics::applyMarioIK(ikRequest);

        // if (marioRightArmBusy()) return;
        // ikRequest.limb = InverseKinematics::MarioIKRequest::Limb::RightArm;
        // ikRequest.targetTransform.pos = target + right * 0.05f;
        // InverseKinematics::applyMarioIK(ikRequest);
    }

    void updateMarioPosition(Engine::Entity* marioEntity, Vec3 newPos) {
        if (!marioEntity) return;

        marioEntity->pos = newPos;
        
        // Filthy hack to work around the fact that we're bypassing Halo's entity movement bookkeeping.
        // I suspect this is updating things like cluster and wake/sleep state.
        Engine::Scripting::submit("(objects_attach (player0) \"\" mario_model \"\")");
        Engine::Scripting::submit("(objects_detach (player0) mario_model)");
    }

    void updatePose( uint32_t entityHandle, Engine::Entity* marioEntity) {
        if (!marioEntity) return;

        // For now, just set all world bone transforms to identity rotation. Keep translation and scale.
        auto worldBones = marioEntity->worldBones.get(marioEntity, 0);
        if (!worldBones) return;

        updateMarioPosition(marioEntity, marioPose[0].pos);
        Vec3 marioVelocity = *(Vec3*)&marioState.velocity[0];
        marioEntity->vel = Coordinates::marioToHalo(marioVelocity);

        // Todo: Move this step to the end of MarioSkeleton::updateMarioPose
        IKToWeapon();

        auto boneCount = marioEntity->worldBones.count();
        for (int i = 0; i < boneCount; i++) {
            auto& bone = worldBones[i];
            bone.w = marioPose[i].w;
            bone.x = marioPose[i].x;
            bone.y = marioPose[i].y;
            bone.z = marioPose[i].z;
            bone.pos = marioPose[i].pos;
        }
    }

    void updateWeaponPose( uint32_t weaponHandle ) {
        auto rec = Engine::getEntityRecord( weaponHandle );
        if (!rec) return;
        auto weaponEntity = rec->entity();
        if (!weaponEntity) return;

        auto weaponBones = weaponEntity->worldBones.get(weaponEntity, 0);
        if (!weaponBones) return;

        auto leftHandBone = getMarioBoneByName("left_hand");

        auto weaponRootBone = &weaponBones[0];
        auto rootBoneInitial = weaponRootBone[0];
        
        MarioWeaponOffset::Offset off;
        MarioWeaponOffset::getWeaponOffset(weaponHandle, off);
        weaponRootBone->pos = leftHandBone.transformPoint(off);
        if (marioArmsBusy()) {
            weaponRootBone->x = leftHandBone.x;
            weaponRootBone->y = leftHandBone.y;
            weaponRootBone->z = leftHandBone.z;
        }

        auto initialInverse = Engine::inverseWorldTransform(rootBoneInitial);
        auto relativeTransform = Engine::multiplyWorldTransforms(
            *weaponRootBone,
            initialInverse
        );
        auto boneCount = weaponEntity->worldBones.count();
        for (int i = 1; i < boneCount; i++) {
            auto& bone = weaponBones[i];
            bone = Engine::multiplyWorldTransforms(relativeTransform, bone);
        }
    }

    uint32_t marioHandle  = 0xFFFFFFFF;

    void processEntity(uint32_t entityHandle, Engine::Entity* entity) {
        if (!entity) return;
        if (isMario(entity)) {
            marioHandle = entityHandle;
            updatePose(entityHandle, entity);
        }
        if (entityHandle == playerWeaponHandle()) {
            updateWeaponPose(entityHandle);
        }
    }

    void renderEntity(Engine::RenderEntityRequest *request, Engine::renderEntity_t renderEntityOriginal) {
        // if (!isMario(request->entityHandle)) return;

        auto renderedHandle = request->entityHandle;
        auto playerHandle = Engine::getPlayerHandle();
        if (renderedHandle != playerHandle) return;
        
        uint32_t weaponHandle = playerWeaponHandle();
        if (weaponHandle != 0xFFFFFFFF) {
            Engine::RenderEntityRequest childRequest = *request;
            childRequest.entityHandle = weaponHandle;
            renderEntityOriginal(&childRequest);
        }

        if (marioHandle != 0xFFFFFFFF) {
            Engine::RenderEntityRequest marioRequest = *request;
            marioRequest.entityHandle = marioHandle;
            renderEntityOriginal(&marioRequest);
        }
    }
}
