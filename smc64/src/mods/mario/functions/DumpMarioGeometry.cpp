#include <cstdio>
#include <string>
#include <stdint.h>
#include <cmath>
#include "libsm64.h"
#include "decomp/surface_terrains.h"
#include "math/Vectors.hpp"
#include "../MarioSkeleton.hpp"
#include "../Coordinates.hpp"
#include "engine/halo1.hpp"

namespace Mod::Mario {

    static uint32_t createDumpSpawnPlatform(float x, float y, float z) {
        int32_t fx = (int32_t)std::lround(x);
        int32_t fy = (int32_t)std::lround(y) - 200;
        int32_t fz = (int32_t)std::lround(z);
        int32_t half = 2000;
        SM64Surface floor[2] = {};
        for (auto& s : floor) { s.type = SURFACE_DEFAULT; s.force = 0; s.terrain = TERRAIN_GRASS; }
        floor[0].vertices[0][0] = -half; floor[0].vertices[0][1] = fy; floor[0].vertices[0][2] = -half;
        floor[0].vertices[1][0] = -half; floor[0].vertices[1][1] = fy; floor[0].vertices[1][2] =  half;
        floor[0].vertices[2][0] =  half; floor[0].vertices[2][1] = fy; floor[0].vertices[2][2] =  half;
        floor[1].vertices[0][0] = -half; floor[1].vertices[0][1] = fy; floor[1].vertices[0][2] = -half;
        floor[1].vertices[1][0] =  half; floor[1].vertices[1][1] = fy; floor[1].vertices[1][2] =  half;
        floor[1].vertices[2][0] =  half; floor[1].vertices[2][1] = fy; floor[1].vertices[2][2] = -half;
        SM64SurfaceObject obj = {};
        obj.transform.position[0] = (float)fx;
        obj.transform.position[1] = 0.0f;
        obj.transform.position[2] = (float)fz;
        obj.surfaceCount = 2;
        obj.surfaces = floor;
        return sm64_surface_object_create(&obj);
    }

    void dumpMarioGeometry() {
        struct SM64MarioInputs marioInputs = {0};
        struct SM64MarioState marioState = {0};
        struct SM64MarioGeometryBuffers marioGeometry = {0};
        struct SM64Matrix4f* marioBoneMatrices = nullptr;

        // Resolve spawn position in Mario-space before creating Mario.
        float spawnX = 0.0f, spawnY = 0.0f, spawnZ = 0.0f;
        auto playerPos = Engine::getPlayerPosition();
        if (playerPos.has_value()) {
            auto mp = Coordinates::haloToMario(playerPos.value());
            spawnX = mp.x; spawnY = mp.y; spawnZ = mp.z;
        }

        uint32_t platformId = createDumpSpawnPlatform(spawnX, spawnY, spawnZ);

        int32_t marioId = sm64_mario_create(spawnX, spawnY, spawnZ);
        if (marioId < 0) {
            printf("Failed to create Mario instance for dumping geometry.\n");
            sm64_surface_object_delete(platformId);
            return;
        }
        std::cout << "Spawn position: (" << spawnX << ", " << spawnY << ", " << spawnZ << ")\n";

        // Initialize Mario geometry buffers.
        { 
            marioGeometry.position = (float*)malloc(sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES);
            marioGeometry.color = (float*)malloc(sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES);
            marioGeometry.normal = (float*)malloc(sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES);
            marioGeometry.uv = (float*)malloc(sizeof(float) * 6 * SM64_GEO_MAX_TRIANGLES);
            marioGeometry.numTrianglesUsed = 0;

            marioBoneMatrices = (struct SM64Matrix4f*)malloc(sizeof(struct SM64Matrix4f) * SM64_MARIO_BONE_COUNT);
        }

        sm64_mario_tick(marioId, &marioInputs, &marioState, &marioGeometry, marioBoneMatrices);
        sm64_mario_tick(marioId, &marioInputs, &marioState, &marioGeometry, marioBoneMatrices);
        sm64_mario_tick(marioId, &marioInputs, &marioState, &marioGeometry, marioBoneMatrices);

        sm64_surface_object_delete(platformId);

        Vec3 marioPos = Vec3{
            marioState.position[0],
            marioState.position[1],
            marioState.position[2]
        };

        std::string folderPath = "C:\\code\\projects\\hacking\\smc64\\data\\";
        // Write position CSV
        {
            std::string filePath = folderPath + "mario.csv";
            FILE* f = fopen(filePath.c_str(), "w");
            if (f) {
                for (uint16_t i = 0; i < marioGeometry.numTrianglesUsed * 3; i++) {
                    float x = marioGeometry.position[i * 3 + 0] - marioPos.x;
                    float y = marioGeometry.position[i * 3 + 1] - marioPos.y;
                    float z = marioGeometry.position[i * 3 + 2] - marioPos.z;

                    float r = marioGeometry.color[i * 3 + 0];
                    float g = marioGeometry.color[i * 3 + 1];
                    float b = marioGeometry.color[i * 3 + 2];

                    float nx = marioGeometry.normal[i * 3 + 0];
                    float ny = marioGeometry.normal[i * 3 + 1];
                    float nz = marioGeometry.normal[i * 3 + 2];

                    float u = marioGeometry.uv[i * 2 + 0];
                    float v = marioGeometry.uv[i * 2 + 1];

                    fprintf(f, "%.6f,%.6f,%.6f, %.6f,%.6f,%.6f, %.6f,%.6f,%.6f, %.6f,%.6f\n",
                        x, y, z, r, g, b, nx, ny, nz, u, v
                    );
                }
                fclose(f);
            }
        }

        // Write skeleton CSV
        {
            std::string filePath = folderPath + "mario_skeleton.csv";
            FILE* f = fopen(filePath.c_str(), "w");
            if (f) {
                // dumpSkeleton(marioGeometry, marioPos, f); // <- This is legacy now. Ignore it.

                for (int i = 0; i < SM64_MARIO_BONE_COUNT; i++) {
                    const SM64Matrix4f& m = marioBoneMatrices[i];

                    Vec3 pos = Vec3{ m.m[3][0], m.m[3][1], m.m[3][2] } - marioPos;
                    Vec3 x = Vec3{ m.m[0][0], m.m[0][1], m.m[0][2] };
                    Vec3 y  = Vec3{ m.m[1][0], m.m[1][1], m.m[1][2] };
                    Vec3 z  = Vec3{ m.m[2][0], m.m[2][1], m.m[2][2] };

                    Vec3 xN = x.normalize();
                    Vec3 yN = y.normalize();
                    Vec3 zN = z.normalize();

                    auto printVec3 = [&](const Vec3& v) {
                        fprintf(f, "%.6f,%.6f,%.6f, ", v.x, v.y, v.z);
                    };

                    printVec3(pos);
                    printVec3(xN);
                    printVec3(yN);

                    fprintf(f, "\n");
                }

                fclose(f);
            }
        }

        // Free resources
        {
            free(marioGeometry.position);
            free(marioGeometry.color);
            free(marioGeometry.normal);
            free(marioGeometry.uv);
            free(marioBoneMatrices);
            sm64_mario_delete(marioId);
        }
    }
}
