#include "engine/halo1.hpp"
#include "spark/events/TeleportPlayer.hpp"

namespace Mod::DevTools {

    void teleportToCrosshair() {
        auto player = Engine::getPlayerEntity();
        if (!player) return;
        auto camera = Engine::getPlayerCameraPointer();
        if (!camera) return;

        Engine::RaycastResult result;
        Engine::raycastPlayerCrosshair(&result);
        if (result.hitType == Engine::HitType_Nothing) return;

        Spark::teleportPlayer.dispatch(result.pos - camera->fwd * 0.5f);
    }

}
