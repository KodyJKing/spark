#include "FreecamMod.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/input/Bindings.hpp"
#include "engine/halo1.hpp"
#include "memory/Memory.hpp"
#include "math/Vectors.hpp"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

static void updateControls(Engine::Camera* camera) {
    float speed = 0.2f;
    if (Spark::Input::actionState("freecam:fast") & 0x80) speed *= 5.0f;
    if (Spark::Input::actionState("freecam:slow") & 0x80) speed *= 0.2f;

    Vec3 fwd   = camera->fwd;
    Vec3 up    = camera->up;
    Vec3 right = fwd.cross(up);

    camera->pos += fwd   * (Spark::Input::actionAxis("freecam:forward")  * speed);
    camera->pos -= fwd   * (Spark::Input::actionAxis("freecam:backward") * speed);
    camera->pos -= right * (Spark::Input::actionAxis("freecam:left")     * speed);
    camera->pos += right * (Spark::Input::actionAxis("freecam:right")    * speed);
    camera->pos += up    * (Spark::Input::actionAxis("freecam:up")       * speed);
    camera->pos -= up    * (Spark::Input::actionAxis("freecam:down")     * speed);
}

void FreecamMod::init() {
    Spark::Input::addAction("freecam:toggle", DIK_HOME);
    Spark::Input::addAction("freecam:fast",   SPARK_GAMEPAD_RIGHT_SHOULDER);
    Spark::Input::addAction("freecam:slow",   SPARK_GAMEPAD_LEFT_SHOULDER);
    
    const Spark::Input::ButtonCode kForward[]  = { DIK_W, SPARK_GAMEPAD_LEFT_STICK_UP };
    const Spark::Input::ButtonCode kBackward[] = { DIK_S, SPARK_GAMEPAD_LEFT_STICK_DOWN };
    const Spark::Input::ButtonCode kLeft[]     = { DIK_A, SPARK_GAMEPAD_LEFT_STICK_LEFT };
    const Spark::Input::ButtonCode kRight[]    = { DIK_D, SPARK_GAMEPAD_LEFT_STICK_RIGHT };
    const Spark::Input::ButtonCode kUp[]       = { DIK_R, SPARK_GAMEPAD_RIGHT_TRIGGER };
    const Spark::Input::ButtonCode kDown[]     = { DIK_F, SPARK_GAMEPAD_LEFT_TRIGGER };
    Spark::Input::addAction("freecam:forward",  kForward,  2);
    Spark::Input::addAction("freecam:backward", kBackward, 2);
    Spark::Input::addAction("freecam:left",     kLeft,     2);
    Spark::Input::addAction("freecam:right",    kRight,    2);
    Spark::Input::addAction("freecam:up",       kUp,       2);
    Spark::Input::addAction("freecam:down",     kDown,     2);

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

    updateControls(camera);
}
