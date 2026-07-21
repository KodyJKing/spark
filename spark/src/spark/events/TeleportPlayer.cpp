#define SPARK_EXPORTS
#include "../EventBus.hpp"
#include "math/Vectors.hpp"
#include "engine/halo1.hpp"
#include "TeleportPlayer.hpp"

namespace Spark {    
    EventBus<void, Vec3> teleportPlayer = EventBus<void, Vec3>(+[](void* ctx, Vec3 position) {
        auto playerEntity = Engine::getPlayerEntity();
        auto camera = Engine::getPlayerCameraPointer();
        if (playerEntity) playerEntity->pos = position;
        if (camera) camera->pos = position;
    });
}
