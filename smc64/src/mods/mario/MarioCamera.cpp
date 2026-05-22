#include "MarioCamera.hpp"
#include "hook/Hooks.hpp"
#include "memory/Memory.hpp"

namespace HaloCE::Mod::Mario::MarioCamera {

    static bool     active               = false;
    static Vec3     cameraPosition       = {0, 0, 0};
    static Vec3     cameraVelocity       = {0, 0, 0};
    static uint32_t framesSinceLastUpdate = 0;

    static Vec3 getCameraPosition() {
        // Extrapolate camera on non-update frames. Without this, the camera jitters terribly.
        float dt = (framesSinceLastUpdate % 2) * 0.5f;
        auto camera = Engine::getPlayerCameraPointer();
        if (!camera) return Vec3{0, 0, 0};
        Vec3 result = cameraPosition
            + cameraVelocity * dt
            + camera->fwd * -1.0f
            + camera->fwd.cross(camera->up) * 0.25f
            + Vec3{0, 0, 0.5f};
        framesSinceLastUpdate++;
        return result;
    }

    void onUpdate(Vec3 marioWorldPos) {
        cameraVelocity  = marioWorldPos - cameraPosition;
        cameraPosition  = marioWorldPos;
        framesSinceLastUpdate = 0;
        active = true;
    }

    void onDisable() {
        active = false;
    }

    void registerHandlers(ModId modId) {
        RenderFPVModel::addHandler(modId, +[](void* /*ctx*/, RenderFPVModel::Cursor next) {
            if (active) return;
            next();
        }, nullptr, 10);

        // Suppress walk input when Mario is possessing the player.
        UpdatePlayerControls::addHandler(modId, +[](void* /*ctx*/, UpdatePlayerControls::Cursor next, float* param_1, float* param_2) {
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
        UpdateCamera::addHandler(modId, +[](void* /*ctx*/, UpdateCamera::Cursor next, float unknown) {
            next(unknown);
            if (!active) return;
            auto camera = Engine::getPlayerCameraPointer();
            if (!camera || !Memory::isAllocated(camera)) return;
            camera->pos = getCameraPosition();
        }, nullptr, 10);
    }

}
