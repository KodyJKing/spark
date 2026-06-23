#include "MarioInverseKinematics.hpp"

namespace Mod::Mario::InverseKinematics {

    void inferBoneLengths(IKRequest& request) {
        for (size_t i = 0; i < request.bones.size() - 1; i++) {
            Vec3 start = request.bones[i].initialTransform.pos;
            Vec3 end = request.bones[i + 1].initialTransform.pos;
            request.bones[i].length = (end - start).length();
        }
    }

    void setupCurrentTransformsFromInitial(IKRequest& request) {
        for (auto& bone : request.bones) {
            bone.currentTransform = bone.initialTransform;
        }
    }

    void addPivotOffset(IKRequest& request, float sign) {
        for (auto& bone : request.bones) {
            Vec3 worldPivotOffset = bone.currentTransform.x * bone.pivotOffset.x 
                + bone.currentTransform.y * bone.pivotOffset.y 
                + bone.currentTransform.z * bone.pivotOffset.z;
            bone.currentTransform.pos += worldPivotOffset * sign;
            bone.initialTransform.pos += worldPivotOffset * sign;
        }
    }

    void solveLimbIK(IKRequest& request) {

        auto& bones = request.bones;

        addPivotOffset(request, 1.0f);
        inferBoneLengths(request);
        setupCurrentTransformsFromInitial(request);

        auto direction = [](Vec3 from, Vec3 to) { return (to - from).normalize(); };

        auto orthonormalize = [](IKBone& bone) {
            Vec3 xDir = bone.currentTransform.x;
            Vec3 yDir = bone.currentTransform.y;
            Vec3 newZDir = xDir.cross(yDir).normalize();
            Vec3 newYDir = newZDir.cross(xDir).normalize();

            if (newZDir.length() < 0.001f || newYDir.length() < 0.001f) {
                // Degenerate case, use z as reference instead.
                Vec3 zDir = bone.currentTransform.z;
                newYDir = zDir.cross(xDir).normalize();
                newZDir = xDir.cross(newYDir).normalize();
            }

            bone.currentTransform.y = newYDir;
            bone.currentTransform.z = newZDir;
        };

        auto forwardUpdate = [&](IKBone& bone, Vec3 target, float length, float sign) {
            Vec3 dir = direction(bone.currentTransform.pos, target);
            bone.currentTransform.pos = target - dir * length;
            bone.currentTransform.x = dir * sign;
            orthonormalize(bone);
        };

        auto backwardUpdate = [&](IKBone& bone, Vec3 target) {
            forwardUpdate(bone, target, bone.length, 1.0f);
        };

        // Solve via FABRIK. No joint constraints for now, just length constraints.
        const int iterationLimit = 10;
        for (int i = 0; i < iterationLimit; i++) {
            backwardUpdate(bones.back(), request.targetTransform.pos);
            for (int j = (int)bones.size() - 2; j >= 0; j--) {
                backwardUpdate(bones[j], bones[j + 1].currentTransform.pos);
            }
            // Forwards pass: set root to initial position and move up the chain.
            bones[0].currentTransform.pos = bones[0].initialTransform.pos;
            for (size_t j = 1; j < bones.size(); j++) {
                forwardUpdate(bones[j], bones[j-1].currentTransform.pos, bones[j - 1].length, -1.0f);
            }
        }

        addPivotOffset(request, -1.0f);

        if (request.enforceRotation) {
            auto& end = bones.back();
            end.currentTransform.x = request.targetTransform.x;
            end.currentTransform.y = request.targetTransform.y;
            end.currentTransform.z = request.targetTransform.z;
        }
    }

    void applyMarioIK(MarioIKRequest& request) {
        std::vector<const char*> boneNames;
        std::vector<Vec3> pivotOffsets;
        if (request.limb == MarioIKRequest::Limb::LeftArm) {
            boneNames = { "left_arm", "left_forearm", "left_hand" };
            pivotOffsets = {
                Vec3{ 0.01f, 0.01f, 0.01f },
                Vec3{ 0.01f, 0.01f, 0.01f },
                Vec3{ 0.01f, 0.0f, 0.00f },
            };
        } else {
            boneNames = { "right_arm", "right_forearm", "right_hand" };
            pivotOffsets = {
                Vec3{ 0.0f, -0.01f, 0.01f },
                Vec3{ 0.0f, -0.01f, 0.01f },
                Vec3{ 0.0f, 0.0f, 0.0f },
            };
        }

        IKRequest ikRequest;
        for (int i = 0; i < (int)boneNames.size(); i++) {
            Engine::Transform initialTransform = getMarioBoneByName(boneNames[i]);
            float length = i == 2 ? 0.05f : -1.0f;
            Vec3 pivotOffset = pivotOffsets[i];
            ikRequest.bones.push_back({ initialTransform, length, pivotOffset });
        }

        ikRequest.targetTransform = request.targetTransform;
        ikRequest.enforceRotation = request.enforceRotation;
        solveLimbIK(ikRequest);

        // Apply solved IK transforms back to Mario's skeleton.
        for (size_t i = 0; i < ikRequest.bones.size(); i++) {
            const auto& ikBone = ikRequest.bones[i];
            const char* boneName = boneNames[i];
            Engine::Transform* marioBone = getMarioBonePointerByName(boneName);
            if (marioBone) {
                *marioBone = ikBone.currentTransform;
            }
        }
    }

}
