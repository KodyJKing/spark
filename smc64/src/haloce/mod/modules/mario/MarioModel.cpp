#include "Mario.hpp"
#include "MarioModel.hpp"
#include "MarioSkeleton.hpp" 
#include "MarioInverseKinematics.hpp"
#include <string>
#include "decomp/sm64.h"
#include "Coordinates.hpp"

namespace {
    void updateEntityRegion(uint32_t entityHandle, void* unknown) {
        typedef uint64_t (*updateEntityRegion_t)( uint32_t entityHandle, void* unknown );
        auto halo1Dll = Engine::dllBase();
        auto pUpdateEntityRegion = (updateEntityRegion_t) (halo1Dll + 0xB368B0U);
        pUpdateEntityRegion(entityHandle, unknown);
    }
}

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

    bool marioArmsBusy() {
        if (marioState.action == ACT_IDLE) return false;
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
        auto target = base + fwd - right * 0.5f;

        InverseKinematics::MarioIKRequest ikRequest;
        ikRequest.limb = InverseKinematics::MarioIKRequest::Limb::LeftArm;
        ikRequest.targetPosition = target;
        InverseKinematics::applyMarioIK(ikRequest);

        if (marioRightArmBusy()) return;

        ikRequest.limb = InverseKinematics::MarioIKRequest::Limb::RightArm;
        ikRequest.targetPosition = target + right * 0.05f;
        InverseKinematics::applyMarioIK(ikRequest);
    }

    void updatePose( uint32_t entityHandle, Engine::Entity* marioEntity) {
        if (!marioEntity) return;

        // For now, just set all world bone transforms to identity rotation. Keep translation and scale.
        auto worldBones = marioEntity->worldBones.get(marioEntity, 0);
        if (!worldBones) return;

        marioEntity->pos = marioPose[0].pos;
        Vec3 marioVelocity = *(Vec3*)&marioState.velocity[0];
        marioEntity->vel = Coordinates::marioToHalo(marioVelocity);
        updateEntityRegion(entityHandle, nullptr);

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
        // This is a test, just place all weapon bones at mario's root bone position.
        auto rec = Engine::getEntityRecord( weaponHandle );
        if (!rec) return;
        auto entity = rec->entity();
        if (!entity) return;

        auto worldBones = entity->worldBones.get(entity, 0);
        if (!worldBones) return;

        auto leftHandBone = getMarioBoneByName("left_hand");

        auto rootBone = &worldBones[0];
        auto rootBoneInitial = rootBone[0];
        if (marioArmsBusy()) {
            rootBone->x = leftHandBone.x;
            rootBone->y = leftHandBone.y;
            rootBone->z = leftHandBone.z;
            rootBone->pos = leftHandBone.pos;
        }
        rootBone->pos = leftHandBone.pos + leftHandBone.x * 0.05f + rootBone->z * 0.0125f;

        auto initialInverse = Engine::inverseWorldTransform(rootBoneInitial);
        auto relativeTransform = Engine::multiplyWorldTransforms(
            *rootBone,
            initialInverse
        );
        auto boneCount = entity->worldBones.count();
        for (int i = 1; i < boneCount; i++) {
            auto& bone = worldBones[i];
            bone = Engine::multiplyWorldTransforms(relativeTransform, bone);
        }

        // auto boneCount = entity->worldBones.count();
        // for (int i = 0; i < boneCount; i++) {
        //     // worldBones[i].w = 0.75f;
        //     if (marioArmsBusy()) {
        //         worldBones[i].x = leftHandBone.x;
        //         worldBones[i].y = leftHandBone.y;
        //         worldBones[i].z = leftHandBone.z;
        //     }
        //     worldBones[i].pos = leftHandBone.pos + leftHandBone.x * 0.05f + worldBones[i].z * 0.025f;
        // }

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

        // When the Mario entity is far from home, it may not recieve a pose update tick. 
        // In lieu of a cleaner solution, I'm having the playedr update tick also update Mario.
        auto playerHandle = Engine::getPlayerHandle();
        if (entityHandle == playerHandle) {
            if (marioHandle != 0xFFFFFFFF) {
                auto marioRec = Engine::getEntityRecord( marioHandle );
                if (!marioRec) return;
                auto marioEntity = marioRec->entity();
                if (!marioEntity) return;
                updatePose(marioHandle, marioEntity);
            }
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
