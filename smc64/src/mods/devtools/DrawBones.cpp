#include "mods/devtools/DrawBones.hpp"
#include "spark/overlay/ESP.hpp"

namespace Mod::DevTools {
    void drawBones(Engine::Entity* entity) {
        namespace ESP = Spark::Overlay::ESP;
        Camera &camera = ESP::camera;

        auto boneCount = entity->worldBones.count();
        Engine::WorldTransform* worldBones = entity->worldBones.get( entity, 0 );

        for ( uint16_t i = 0; i < boneCount; i++ ) {
            auto& bone = worldBones[i];

            Vec3 pos = bone.pos;

            float r = 0.0125f;

            ESP::drawLine(pos, pos + bone.x * r, IM_COL32(255, 0, 0, 255)); // Forward - Red
            ESP::drawLine(pos, pos + bone.y * r, IM_COL32(0, 255, 0, 255)); // Left - Green
            ESP::drawLine(pos, pos + bone.z * r, IM_COL32(0, 0, 255, 255)); // Up - Blue

            char indexText[16];
            snprintf(indexText, sizeof(indexText), "%d", i);
            ESP::drawText(pos, indexText, IM_COL32(255, 255, 255,  128));
        }

                // Draw white point at root bone position
        if (boneCount > 0) {
            auto rootBone = worldBones[0];
            ESP::drawCircle(rootBone.pos, 0.005f, IM_COL32(255, 255, 255, 255), true);
        }
    }
}
