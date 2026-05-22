#include "mods/devtools/DrawEntityCollision.hpp"
#include "overlay/ESP.hpp"
#include "DrawBSP.hpp"

namespace Mod::DevTools {
    void drawEntityCollision(Engine::Entity* entity) {
        namespace ESP = Overlay::ESP;
        Camera &camera = ESP::camera;

        auto collisionTag = getCollisionGeometryTag(entity->tag());
        if (collisionTag == nullptr) return;
        auto collisionData = (Engine::CollisionTagData*) collisionTag->getData();
        if (collisionData == nullptr) return;
        
        auto nodeCount = collisionData->collisionNodes.count;
        for (uint16_t i = 0; i < nodeCount; i++) {
            auto node = collisionData->collisionNodes.get<Engine::CollisionNode>(i);
            if (node == nullptr) continue;

            auto bsp = node->collisionBsps.get<Engine::CollisionBSP>(0);
            if (bsp == nullptr) continue;

            auto bone = entity->worldBones.get(entity, i);
            Vec3 origin = bone->pos;
            Vec3 x = bone->x;
            Vec3 y = bone->y;
            Vec3 z = bone->z;

            Mod::DevTools::drawBSP(bsp, origin, x, y, z);
        }
    }
}
