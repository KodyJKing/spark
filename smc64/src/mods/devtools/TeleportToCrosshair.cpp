#include "engine/halo1.hpp"

namespace Mod::DevTools {

    void teleportToCrosshair() {
        auto player = Engine::getPlayerEntity();
        if (!player) return;
        auto camera = Engine::getPlayerCameraPointer();
        if (!camera) return;

        Engine::RaycastResult result;
        Engine::raycastPlayerCrosshair(&result);
        if (result.hitType == Engine::HitType_Nothing) return;

        player->pos = result.pos - camera->fwd * 0.5f;
        camera->pos = player->pos; // In case a free camera is active.
    }

}
