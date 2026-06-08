#include "mods/devtools/DrawEntity.hpp"

#include "DrawBones.hpp"
#include "DrawEntityCollision.hpp"
#include "spark/overlay/ESP.hpp"
#include "DrawEntity.hpp"

namespace Mod::DevTools {

    void drawEntityFrame(Engine::Entity* entity) {
        auto camera = Spark::Overlay::ESP::camera;
        
        // Draw up and forward vectors
        auto pos = entity->pos;
        auto fwd = entity->fwd;
        auto up = entity->up;

        float r = 0.025f;
        
        Spark::Overlay::ESP::drawLine(pos, pos + fwd * r, IM_COL32(0, 255, 255, 255)); // Forward - Cyan
        Spark::Overlay::ESP::drawLine(pos, pos + up * r, IM_COL32(255, 255, 0, 255)); // Up - Yellow
    }
    
    void drawEntity(Engine::Entity* entity) {
        drawEntityFrame(entity);
        // Draw bones
        drawBones(entity);
        // Draw collision
        // drawEntityCollision(entity);
    }
}
