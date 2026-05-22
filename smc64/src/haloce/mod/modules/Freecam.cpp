#include "freecam.hpp"
#include "common.hpp"
#include "hook/Hooks.hpp"
#include <Xinput.h>
#pragma comment( lib, "Xinput.lib" )

namespace HaloCE::Freecam {

    bool isFreecamEnabled = false;

    bool isEnabled() {
        return isFreecamEnabled;
    }

    void registerHandlers() {
        RenderFPVModel::addHandler(0, [](RenderFPVModel::Next next) {
            if (isEnabled()) return;
            next();
        });

        // void updatePlayerControls(undefined4 *param_1, undefined4 *param_2)
        UpdatePlayerControls::addHandler(0, [](UpdatePlayerControls::Next next, float* param_1, float* param_2) {
            if (!isEnabled()) {
                next(param_1, param_2);
                return;
            }
            auto playerController = Halo1::getPlayerControllerPointer();
            if (!playerController || !Memory::isAllocated(playerController)) return;

            // Backup player controller state
            Halo1::PlayerController pc = *playerController;
            // playerController->actions = 0;
            playerController->walkX = 0.0f;
            playerController->walkY = 0.0f;
            // playerController->gunTrigger = 0.0f;
            next(param_1, param_2);
            // Restore player controller state
            *playerController = pc;
        });

        // Slight misnomer: This function updates *all* cameras, not a single camera.
        UpdateCamera::addHandler(0, [](UpdateCamera::Next next, float unknown) {
            if (GetAsyncKeyState(VK_F10)) {
                return next(unknown);
            }
            if (!isEnabled()) {
                return next(unknown);
            }

            auto camera = Halo1::getPlayerCameraPointer();
            bool camAllocated = camera && Memory::isAllocated(camera);
            Vec3 camPos = {0, 0, 0};
            bool saveCamPos = camAllocated && isFreecamEnabled;

            if (saveCamPos) camPos = camera->pos;
            next(unknown);
            if (saveCamPos) {
                camera->pos = camPos;
            }
        });
    }

    void updateXboxControls(Halo1::Camera* camera) {
        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(XINPUT_STATE));
        XInputGetState(0, &state);

        float speed = 0.2f;
        if (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
            speed *= 5.0f; // Fast
        if (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
            speed *= 0.2f; // Slow
        
        Vec3 fwd = camera->fwd;
        Vec3 up = camera->up;
        Vec3 right = fwd.cross( up );

        Vec3 moveDelta = {};

        // Left stick for horizontal movement.
        moveDelta += fwd * ( state.Gamepad.sThumbLY / 32768.0f * speed );
        moveDelta += right * ( state.Gamepad.sThumbLX / 32768.0f * speed );

        // L/R triggers for vertical movement.
        moveDelta += up * ( (state.Gamepad.bRightTrigger / 255.0f) * speed );
        moveDelta -= up * ( (state.Gamepad.bLeftTrigger / 255.0f) * speed );
    
        camera->pos += moveDelta;
    }

    void updateKeyboardControls(Halo1::Camera* camera) {

        float speed = 0.2f;
        if (GetAsyncKeyState(VK_MENU) & 0x8000)
            speed *= 5.0f; // Fast
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
            speed *= 0.2f; // Slow
        
        Vec3 fwd = camera->fwd;
        Vec3 up = camera->up;
        Vec3 right = fwd.cross( up );

        // WASD for horizontal movement.
        if (GetAsyncKeyState('W') & 0x8000)
            camera->pos += fwd * speed;
        if (GetAsyncKeyState('S') & 0x8000)
            camera->pos -= fwd * speed;
        if (GetAsyncKeyState('A') & 0x8000)
            camera->pos -= right * speed;
        if (GetAsyncKeyState('D') & 0x8000)
            camera->pos += right * speed;
        
        // QE for vertical movement.
        if (GetAsyncKeyState('R') & 0x8000)
            camera->pos += up * speed;
        if (GetAsyncKeyState('F') & 0x8000)
            camera->pos -= up * speed;
    }

    void update() {
        if (GetAsyncKeyState(VK_HOME) & 1) {
            isFreecamEnabled = !isFreecamEnabled;
            std::cout << "Freecam " << (isFreecamEnabled ? "enabled." : "disabled.") << std::endl;
        }
        if (!isEnabled()) return;

        auto camera = Halo1::getPlayerCameraPointer();
        if (!camera || !Memory::isAllocated(camera)) return;

        if (isFreecamEnabled) {
            updateXboxControls(camera);
            updateKeyboardControls(camera);
        } else {
            // camera->pos = cameraOverride.position;
        }

    }
    
}
