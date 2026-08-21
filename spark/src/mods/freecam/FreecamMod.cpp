#include "FreecamMod.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/input/Bindings.hpp"
#include "engine/halo1.hpp"
#include "memory/Memory.hpp"
#include "math/Vectors.hpp"
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <Xinput.h>

static void updateXboxControls(Engine::Camera* camera) {
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));
    XInputGetState(0, &state);

    float speed = 0.2f;
    if (Spark::Input::actionState("freecam:fast") & 0x80) speed *= 5.0f;
    if (Spark::Input::actionState("freecam:slow") & 0x80) speed *= 0.2f;

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
    if (Spark::Input::actionState("freecam:fast") & 0x80) speed *= 5.0f;
    if (Spark::Input::actionState("freecam:slow") & 0x80) speed *= 0.2f;

    Vec3 fwd   = camera->fwd;
    Vec3 up    = camera->up;
    Vec3 right = fwd.cross(up);

    if (Spark::Input::actionState("freecam:forward") & 0x80)  camera->pos += fwd   * speed;
    if (Spark::Input::actionState("freecam:backward") & 0x80) camera->pos -= fwd   * speed;
    if (Spark::Input::actionState("freecam:left") & 0x80)     camera->pos -= right * speed;
    if (Spark::Input::actionState("freecam:right") & 0x80)    camera->pos += right * speed;
    if (Spark::Input::actionState("freecam:up") & 0x80)       camera->pos += up    * speed;
    if (Spark::Input::actionState("freecam:down") & 0x80)     camera->pos -= up    * speed;
}

void FreecamMod::init() {
    Spark::Input::addAction("freecam:toggle", DIK_HOME);
    Spark::Input::addAction("freecam:fast",   SPARK_GAMEPAD_RIGHT_SHOULDER);
    Spark::Input::addAction("freecam:slow",   SPARK_GAMEPAD_LEFT_SHOULDER);
    
    Spark::Input::addAction("freecam:forward", DIK_W);
    Spark::Input::addAction("freecam:backward", DIK_S);
    Spark::Input::addAction("freecam:left", DIK_A);
    Spark::Input::addAction("freecam:right", DIK_D);
    Spark::Input::addAction("freecam:up", DIK_R);
    Spark::Input::addAction("freecam:down", DIK_F);

    Spark::RenderFPVModel::addHandler(modId_, +[](void* ctx, auto next) {
        if (static_cast<FreecamMod*>(ctx)->enabled_) return;
        next();
    }, this);

    Spark::UpdatePlayerControls::addHandler(modId_, +[](void* ctx, auto next, float* param_1, float* param_2) {
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

    Spark::UpdateCamera::addHandler(modId_, +[](void* ctx, auto next, float unknown) {
        auto* self = static_cast<FreecamMod*>(ctx);
        if (!self->enabled_)         return next(unknown);
        auto camera       = Engine::getPlayerCameraPointer();
        bool camAllocated = camera && Memory::isAllocated(camera);
        Vec3 camPos       = {0, 0, 0};
        if (camAllocated) camPos = camera->pos;
        next(unknown);
        if (camAllocated) camera->pos = camPos;
    }, this);

    Spark::UpdateAllEntities::addHandler(modId_, +[](void* ctx, auto next) {
        static_cast<FreecamMod*>(ctx)->update();
        next();
    }, this);
}

void FreecamMod::update() {
    static unsigned char prevToggle = 0;
    unsigned char toggleNow = Spark::Input::actionPressed("freecam:toggle", &prevToggle);
    if (toggleNow) enabled_ = !enabled_;
    if (!enabled_) return;

    auto camera = Engine::getPlayerCameraPointer();
    if (!camera || !Memory::isAllocated(camera)) return;

    updateXboxControls(camera);
    updateKeyboardControls(camera);
}
