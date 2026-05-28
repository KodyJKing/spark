#include "Mario.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "spark/overlay/ESP.hpp"

#include "libsm64.h"

#include "MarioState.hpp"
#include "MarioModel.hpp"
#include "MarioSkeleton.hpp"
#include "MarioMelee.hpp"
#include "Coordinates.hpp"

namespace HaloCE::Mod::Mario {

    int32_t highlightTriangleIndex = -1;

    void draw_SM64SurfaceCollisionData(SM64SurfaceCollisionData* surfaceData, ImU32 color) {
        namespace ESP = Spark::Overlay::ESP;
        Camera& camera = ESP::camera;
        Vec3 v1 = Coordinates::marioToHalo(surfaceData->vertex1);
        Vec3 v2 = Coordinates::marioToHalo(surfaceData->vertex2);
        Vec3 v3 = Coordinates::marioToHalo(surfaceData->vertex3);
        ESP::drawLine(v1, v2, color);
        ESP::drawLine(v2, v3, color);
        ESP::drawLine(v3, v1, color);

        Vec3 center = (v1 + v2 + v3) / 3.0f;
        auto n = surfaceData->normal;
        Vec3 normalEnd = center + Vec3{n.x, n.z, n.y} * 0.1f;
        ESP::drawLine(center, normalEnd, color);
        ESP::drawPoint(center, color);
    }

    void marioDebugWindow(SM64WallCollisionData& wallData) {
        ImGui::Begin("Mario Debug Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Mario Model Handle: %X", MarioModel::marioHandle);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right click to copy");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%X", MarioModel::marioHandle);
            ImGui::SetClipboardText(buffer);
        }

        ImGui::Text("Mario ID: %d", marioId);
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", marioState.position[0], marioState.position[1], marioState.position[2]);
        ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", marioState.velocity[0], marioState.velocity[1], marioState.velocity[2]);
        ImGui::Text("Health: %d", marioState.health);
        ImGui::Text("Action: 0x%X", marioState.action);
        ImGui::Text("Flags: 0x%X", marioState.flags);
        ImGui::Text("Num walls: %d", wallData.numWalls);

        ImGui::SliderInt("##Highlight Triangle Index", &highlightTriangleIndex, -1, marioGeometry.numTrianglesUsed - 1);

        ImGui::End();
    }

    void debugRender() {
#ifdef ENABLE_MARIO

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();

        bool highlightEnabled = highlightTriangleIndex >= 0 && highlightTriangleIndex < static_cast<int32_t>(marioGeometry.numTrianglesUsed);

        for (int i = 0; i < marioGeometry.numTrianglesUsed; i++) {
            Vec3* pos = (Vec3*)&marioGeometry.position[i * 3 * 3];
            Vec3* color = (Vec3*)&marioGeometry.color[i * 3 * 3];

            bool highlightThis = highlightEnabled && i == highlightTriangleIndex;
            uint8_t alpha = 20;

            for (int j = 0; j < 3; j++) {
                Vec3 p1 = pos[j];
                Vec3 p2 = pos[(j + 1) % 3];

                Vec3 haloP1 = Coordinates::marioToHalo(p1);
                Vec3 haloP2 = Coordinates::marioToHalo(p2);

                ImU32 colorIm = IM_COL32(
                    static_cast<uint8_t>(color[j].x * 255),
                    static_cast<uint8_t>(color[j].y * 255),
                    static_cast<uint8_t>(color[j].z * 255),
                    alpha
                );

                Spark::Overlay::ESP::drawLine(haloP1, haloP2, colorIm);
            }

            if (highlightThis) {
                Vec3 haloP1 = Coordinates::marioToHalo(pos[0]);
                Vec3 haloP2 = Coordinates::marioToHalo(pos[1]);
                Vec3 haloP3 = Coordinates::marioToHalo(pos[2]);
                Spark::Overlay::ESP::drawPoint(haloP1, IM_COL32(255, 0, 0, 255));
                Spark::Overlay::ESP::drawPoint(haloP2, IM_COL32(0, 255, 0, 255));
                Spark::Overlay::ESP::drawPoint(haloP3, IM_COL32(0, 0, 255, 255));
            }
        }

        SM64SurfaceCollisionData* floorData;
        sm64_surface_find_floor(
            marioState.position[0],
            marioState.position[1],
            marioState.position[2],
            &floorData
        );
        if (floorData)
            draw_SM64SurfaceCollisionData(floorData, IM_COL32(255, 200, 0, 255));

        SM64SurfaceCollisionData* ceilData;
        sm64_surface_find_ceil(
            marioState.position[0],
            marioState.position[1],
            marioState.position[2],
            &ceilData
        );
        if (ceilData)
            draw_SM64SurfaceCollisionData(ceilData, IM_COL32(255, 0, 200, 255));

        SM64WallCollisionData wallData = {};
        wallData.x = marioState.position[0];
        wallData.y = marioState.position[1];
        wallData.z = marioState.position[2];
        wallData.radius = 100.0f;
        wallData.offsetY = 10.0f;
        sm64_surface_find_wall_collisions(&wallData);

        for (int i = 0; i < wallData.numWalls; i++) {
            SM64SurfaceCollisionData* wallSurface = wallData.walls[i];
            if (wallSurface)
                draw_SM64SurfaceCollisionData(wallSurface, IM_COL32(0, 200, 255, 255));
        }

        drawMarioBones(marioGeometry);
        MarioMelee::debugRender();

        marioDebugWindow(wallData);

#endif // ENABLE_MARIO
    }

}
