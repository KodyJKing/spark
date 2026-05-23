#include "FreecamMod.hpp"
#include "spark/hook/Hooks.hpp"
#include "engine/halo1.hpp"
#include "memory/Memory.hpp"
#include "math/Vectors.hpp"
#include <Windows.h>
#include <Xinput.h>
#include <iostream>
#pragma comment(lib, "Xinput.lib")

static void updateXboxControls(Engine::Camera* camera) {
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));
    XInputGetState(0, &state);

    float speed = 0.2f;
    if (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
        speed *= 5.0f;
    if (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
        speed *= 0.2f;

    Vec3 fwd   = camera->fwd;
    Vec3 up    = camera->up;
    Vec3 right = fwd.cross(up);

    Vec3 moveDelta = {};
    moveDelta += fwd   * (state.Gamepad.sThumbLY / 32768.0f * speed);
    moveDelta += right * (state.Gamepad.sThumbLX / 32768.0f * speed);
    moveDelta += up    * (state.Gamepad.bRightTrigger / 255.0f * speed);
    moveDelta -= up    * (state.Gamepad.bLeftTrigger  / 255.0f * speed);

    camera->pos += moveDelta;
}

static void updateKeyboardControls(Engine::Camera* camera) {
    float speed = 0.2f;
    if (GetAsyncKeyState(VK_MENU)    & 0x8000) speed *= 5.0f;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) speed *= 0.2f;

    Vec3 fwd   = camera->fwd;
    Vec3 up    = camera->up;
    Vec3 right = fwd.cross(up);

    if (GetAsyncKeyState('W') & 0x8000) camera->pos += fwd   * speed;
    if (GetAsyncKeyState('S') & 0x8000) camera->pos -= fwd   * speed;
    if (GetAsyncKeyState('A') & 0x8000) camera->pos -= right * speed;
    if (GetAsyncKeyState('D') & 0x8000) camera->pos += right * speed;
    if (GetAsyncKeyState('R') & 0x8000) camera->pos += up    * speed;
    if (GetAsyncKeyState('F') & 0x8000) camera->pos -= up    * speed;
}

void FreecamMod::init() {
    Spark::RenderFPVModel::addHandler(modId_, +[](void* ctx, Spark::RenderFPVModel::Cursor next) {
        if (static_cast<FreecamMod*>(ctx)->enabled_) return;
        next();
    }, this);

    Spark::UpdatePlayerControls::addHandler(modId_, +[](void* ctx, Spark::UpdatePlayerControls::Cursor next, float* param_1, float* param_2) {
        auto* self = static_cast<FreecamMod*>(ctx);
        if (!self->enabled_) {
            next(param_1, param_2);
            return;
        }
        auto playerController = Engine::getPlayerControllerPointer();
        if (!playerController || !Memory::isAllocated(playerController)) return;
        Engine::PlayerController pc = *playerController;
        playerController->walkX = 0.0f;
        playerController->walkY = 0.0f;
        playerController->gunTrigger = 0.0f;
        playerController->actions = 0;
        next(param_1, param_2);
        *playerController = pc;
    }, this);

    Spark::UpdateCamera::addHandler(modId_, +[](void* ctx, Spark::UpdateCamera::Cursor next, float unknown) {
        auto* self = static_cast<FreecamMod*>(ctx);
        if (!self->enabled_)         return next(unknown);
        auto camera       = Engine::getPlayerCameraPointer();
        bool camAllocated = camera && Memory::isAllocated(camera);
        Vec3 camPos       = {0, 0, 0};
        if (camAllocated) camPos = camera->pos;
        next(unknown);
        if (camAllocated) camera->pos = camPos;
    }, this);

    Spark::UpdateAllEntities::addHandler(modId_, +[](void* ctx, Spark::UpdateAllEntities::Cursor next) {
        static_cast<FreecamMod*>(ctx)->update();
        next();
    }, this);
}

void FreecamMod::update() {
    if (GetAsyncKeyState(VK_HOME) & 1) enabled_ = !enabled_;
    if (!enabled_) return;

    auto camera = Engine::getPlayerCameraPointer();
    if (!camera || !Memory::isAllocated(camera)) return;

    updateXboxControls(camera);
    updateKeyboardControls(camera);
}
