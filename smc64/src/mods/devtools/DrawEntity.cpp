#include "mods/devtools/DrawEntity.hpp"

#include "DrawBones.hpp"
#include "DrawEntityCollision.hpp"

namespace Mod::DevTools {
    void drawEntity(Engine::Entity* entity) {
        // Draw bones
        drawBones(entity);
        // Draw collision
        // drawEntityCollision(entity);
    }
}
