#include "MarioCamera.hpp"
#include "spark/hook/Hooks.hpp"
#include "memory/Memory.hpp"
#include "halomcc/HaloMCC.hpp"
#include "MarioState.hpp"
#include <iostream>

namespace HaloCE::Mod::Mario::MarioCamera {

    static bool     active               = false;
    static Vec3     cameraPosition       = {0, 0, 0};
    static Vec3     cameraVelocity       = {0, 0, 0};
    static uint32_t framesSinceLastUpdate = 0;
    static uint32_t lastTickInterval      = 2;
    static float    camDistanceScale      = 1.0f;

    static constexpr float CAM_BACK        = -1.2f;
    static constexpr float CAM_RIGHT       =  0.125f;
    static constexpr float CAM_RIGHT_FIXED =  0.125f;
    static constexpr float CAM_UP          =  0.25f;
    static constexpr float CAM_WALL_MARGIN =  0.1f;
    static constexpr float CAM_EXTEND_RATE =  0.06f; // per frame; ~1 s to full extension at 60 fps


    static Vec3 getCameraPosition() {
        uint32_t effectiveInterval = lastTickInterval;
        if (HaloMCC::isPauseMenuOpen() || effectiveInterval == 0) 
            effectiveInterval = 1;

        // Extrapolate camera on non-update frames. Without this, the camera jitters terribly.
        float dt = (framesSinceLastUpdate) / (float) effectiveInterval;
        if (dt > 1.0f) dt = 1.0f; // Don't extrapolate too far, to avoid going through walls.
        
        auto camera = Engine::getPlayerCameraPointer();
        if (!camera) return Vec3{0, 0, 0};

        Vec3 marioPos = cameraPosition + cameraVelocity * dt;
        Vec3 right    = camera->fwd.cross(camera->up);

        // Cast a ray from Mario's head height toward the desired camera position.
        // This lets us detect walls and pull the camera in to prevent clipping.
        Vec3 rayOrigin = marioPos + Vec3{0, 0, CAM_UP};
        Vec3 rayDisp   = camera->fwd * CAM_BACK + right * CAM_RIGHT;
        float rayLen   = rayDisp.length();

        float targetScale = 1.0f;
        if (rayLen > 0.0f) {
            Engine::RaycastResult rr;
            Engine::raycast(ENGINE_RAYCAST_PROJECTILE_FLAGS, &rayOrigin, &rayDisp,
                            Engine::getPlayerHandle(), &rr);
            if (rr.hitType != Engine::HitType_Nothing) {
                Vec3 rayDir  = rayDisp * (1.0f / rayLen);
                float hitDist = (rr.pos - rayOrigin).dot(rayDir);
                float t = (hitDist - CAM_WALL_MARGIN) / rayLen;
                if (t < 0.0f) t = 0.0f;
                if (t < targetScale) targetScale = t;
            }
        }

        // // Retract immediately; extend gradually so the camera doesn't pop back through walls.
        // if (targetScale < camDistanceScale)
        //     camDistanceScale = targetScale;
        // else if (camDistanceScale < targetScale)
        //     camDistanceScale = camDistanceScale + CAM_EXTEND_RATE < targetScale
        //                      ? camDistanceScale + CAM_EXTEND_RATE
        //                      : targetScale;
        camDistanceScale = 1.0f;

        Vec3 result = marioPos
            + (camera->fwd * CAM_BACK + right * CAM_RIGHT) * camDistanceScale 
            + right * CAM_RIGHT_FIXED
            + Vec3{0, 0, CAM_UP};
        framesSinceLastUpdate++;
        return result;
    }

    void onUpdate(Vec3 marioWorldPos) {
        cameraVelocity  = marioWorldPos - cameraPosition;
        cameraPosition  = marioWorldPos;
        lastTickInterval = framesSinceLastUpdate;
        framesSinceLastUpdate = 0;
        active = true;
    }

    void onDisable() {
        active = false;
        camDistanceScale = 1.0f;
    }

    void registerHandlers(Spark::ModId modId) {
        Spark::RenderFPVModel::addHandler(modId, +[](void* /*ctx*/, auto next) {
            if (active) return;
            next();
        }, nullptr, 10);

        // Suppress walk input when Mario is possessing the player.
        Spark::UpdatePlayerControls::addHandler(modId, +[](void* /*ctx*/, auto next, float* param_1, float* param_2) {
            if (!active) {
                next(param_1, param_2);
                return;
            }
            auto playerController = Engine::getPlayerControllerPointer();
            if (!playerController || !Memory::isAllocated(playerController)) return;
            Engine::PlayerController pc = *playerController;
            playerController->walkX = 0.0f;
            playerController->walkY = 0.0f;
            next(param_1, param_2);
            *playerController = pc;
        }, nullptr, 10);

        // Override camera position with Mario's interpolated position.
        Spark::UpdateCamera::addHandler(modId, +[](void* /*ctx*/, auto next, float unknown) {
            next(unknown);
            if (!active || !marioInControl()) return;
            auto camera = Engine::getPlayerCameraPointer();
            if (!camera || !Memory::isAllocated(camera)) return;
            camera->pos = getCameraPosition();
        }, nullptr, 10);
    }

}
