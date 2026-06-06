#include "Mario.hpp"

#include "libsm64.h"
#include "decomp/sm64.h"
#include "decomp/surface_terrains.h"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <string>
#include <iostream>
#include <mutex>

#include <Windows.h>

#include <filesystem>
#include "engine/halo1.hpp"
#include "MarioInput.hpp"
#include "BSPConversion.hpp"
#include "MarioBSPChunk.hpp"
#include "math/Vectors.hpp"
#include "Coordinates.hpp"
#include "DynamicGeometry.hpp"
#include "MarioFacing.hpp"
#include "MarioGameSpeed.hpp"
#include "MarioShieldRegen.hpp"

#include "MarioAudio.hpp"
#include "MarioCamera.hpp"
#include "ThirdPersonFix.hpp"
#include "MarioPickingFix.hpp"
#include "MarioDamageHook.hpp"
#include "MarioSkeleton.hpp"
#include "MarioModel.hpp"
#include "MarioMelee.hpp"
#include "MarioWeaponOffset.hpp"
#include "MarioAimingIK.hpp"
#include "MarioWeaponKick.hpp"

namespace HaloCE::Mod::Mario {

    // Guards the geometry buffers and full update() body against concurrent free().
    static std::mutex s_updateMutex;

    uint8_t* readRomFile(const char *path, size_t *fileLength) {
        FILE *f = fopen(path, "rb");
        if (!f) {
            printf("\nFailed to read ROM file \"%s\"\n\n", path);
            return nullptr;
        }

        fseek(f, 0, SEEK_END);
        size_t length = (size_t)ftell(f);
        rewind(f);
        uint8_t *buffer = (uint8_t*)malloc(length + 1);
        fread(buffer, 1, length, f);
        buffer[length] = 0;
        fclose(f);

        if (fileLength) *fileLength = length;

        return buffer;
    }

    void debugPrint(const char *msg) {
        printf("%s\n", msg);
    }

    void createSpawnPlatform(const Vec3& localPos) {
        // Create a temporary square floor in mario-local space to support spawn.
        // This is replaced with real BSP geometry immediately after by MarioBSPChunk::init.
        int32_t fx = (int32_t)std::lround(localPos.x);
        int32_t fz = (int32_t)std::lround(localPos.z);
        int32_t fy = (int32_t)std::lround(localPos.y) - 200; // ~0.5m below spawn
        int32_t half = 2000; // ~5m half-extent
        SM64Surface floor[2] = {};
        for (auto& s : floor) { s.type = SURFACE_DEFAULT; s.force = 0; s.terrain = TERRAIN_GRASS; }
        // CCW winding (viewed from above) for upward normal
        floor[0].vertices[0][0] = fx - half; floor[0].vertices[0][1] = fy; floor[0].vertices[0][2] = fz - half;
        floor[0].vertices[1][0] = fx - half; floor[0].vertices[1][1] = fy; floor[0].vertices[1][2] = fz + half;
        floor[0].vertices[2][0] = fx + half; floor[0].vertices[2][1] = fy; floor[0].vertices[2][2] = fz + half;
        floor[1].vertices[0][0] = fx - half; floor[1].vertices[0][1] = fy; floor[1].vertices[0][2] = fz - half;
        floor[1].vertices[1][0] = fx + half; floor[1].vertices[1][1] = fy; floor[1].vertices[1][2] = fz + half;
        floor[1].vertices[2][0] = fx + half; floor[1].vertices[2][1] = fy; floor[1].vertices[2][2] = fz - half;
        sm64_static_surfaces_load(floor, 2);
    }

    void initMario() {
        // Create a Mario instance at Cheif's position (split into chunk + local).
        if (marioId < 0) {

            if (Engine::isPlayerInputDisabled()) {
                printf("initMario: player input is disabled, skipping Mario spawn.\n");
                return;
            }
            
            auto playerPos = Engine::getPlayerPosition();
            if (!playerPos.has_value()) {
                printf("initMario: no player position available, skipping Mario spawn.\n");
                return;
            }
            Vec3 marioWorldPos = Coordinates::haloToMario(playerPos.value());
            marioChunk = Coordinates::marioChunkForPosition(marioWorldPos);
            Vec3 local = Coordinates::marioWorldToLocal(marioWorldPos, marioChunk);
            std::cout << "Local position: (" << local.x << ", " << local.y << ", " << local.z << ")\n";

            // Create temporary spawn platform, then create Mario.
            createSpawnPlatform(local);
            marioId = sm64_mario_create(local.x, local.y, local.z);
        }
        if (marioId < 0) {
            printf("Failed to create Mario instance.\n");
            Beep(750, 100);
            Beep(750, 100);
            return;
        }
        printf("Created Mario instance with ID: %d in chunk (%d, %d, %d)\n",
               marioId, marioChunk.x, marioChunk.y, marioChunk.z);

        // Initialize Mario state and geometry buffers.
        marioInputs = {};
        marioState = {};
        marioGeometry.position = (float*)malloc(sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES);
        marioGeometry.color = (float*)malloc(sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES);
        marioGeometry.normal = (float*)malloc(sizeof(float) * 9 * SM64_GEO_MAX_TRIANGLES);
        marioGeometry.uv = (float*)malloc(sizeof(float) * 6 * SM64_GEO_MAX_TRIANGLES);
        marioGeometry.numTrianglesUsed = 0;

        // Print buffer locations
        printf("Mario geometry buffers initialized:\n");
        printf(" - Position buffer: %p\n", (void*)marioGeometry.position);
        printf(" - Color buffer: %p\n", (void*)marioGeometry.color);
        printf(" - Normal buffer: %p\n", (void*)marioGeometry.normal);
        printf(" - UV buffer: %p\n", (void*)marioGeometry.uv);

        // Now we can load the level for Mario.
        sm64_set_mario_gas_level(marioId, -999999.99f);
        sm64_set_mario_water_level(marioId, -999999.99f);
        MarioBSPChunk::init(marioChunk);
    }

    void deinitMario() {
        if (marioId >= 0) {
            sm64_mario_delete(marioId);
            marioId = -1;
        }
        marioInputs = {};
        marioState = {};
        if (marioGeometry.position) { ::free(marioGeometry.position); marioGeometry.position = nullptr; }
        if (marioGeometry.color)    { ::free(marioGeometry.color);    marioGeometry.color    = nullptr; }
        if (marioGeometry.normal)   { ::free(marioGeometry.normal);   marioGeometry.normal   = nullptr; }
        if (marioGeometry.uv)       { ::free(marioGeometry.uv);       marioGeometry.uv       = nullptr; }
    }

    // Public:
    
    void init(Spark::ModId modId) {
        #ifdef ENABLE_MARIO
        // Get location of host exe file using Windows API
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::filesystem::path modulePath = path;
        std::filesystem::path romPath = modulePath.parent_path() / "baserom.us.z64";
        std::string romPathStr = romPath.string();
        printf("Module path: %s\n", romPathStr.c_str());

        rom = readRomFile(romPathStr.c_str(), &romSize);
        if (rom == nullptr) return;
        texture = (uint8_t*)malloc( 4 * SM64_TEXTURE_WIDTH * SM64_TEXTURE_HEIGHT );

        // Print rom ptr, size and texture ptr
        printf("ROM pointer: %p, size: %zu bytes\n", (void*)rom, romSize);
        printf("Texture pointer: %p\n", (void*)texture);

        sm64_global_terminate();
        sm64_global_init(rom, texture);

        // sm64_register_debug_print_function(debugPrint);

        MarioAudio::init(rom);

        ThirdPersonFix::registerHandlers(modId);
        MarioPickingFix::registerHandlers(modId);
        MarioDamageHook::registerHandlers(modId);
        MarioCamera::registerHandlers(modId);
        MarioWeaponOffset::registerHandlers(modId);
        MarioWeaponKick::registerHandlers(modId);
        #endif
    }

    void free() {
        #ifdef ENABLE_MARIO
        // Join the audio thread first — it holds s_sm64Mutex during sm64_audio_tick.
        // sm64_mario_delete and sm64_global_terminate don't acquire that mutex, so
        // calling them while the audio thread is running causes a race/crash on reinit.
        MarioAudio::free();

        DynamicGeometry::free();
        MarioBSPChunk::free();

        // Hold s_updateMutex while freeing state that update() reads.
        // This ensures any in-flight update() call completes before we pull
        // the buffers out from under it.
        std::lock_guard<std::mutex> updateLock(s_updateMutex);

        deinitMario();

        sm64_global_terminate();

        if (texture) {
            ::free(texture);
            texture = nullptr;
        }
        if (rom) {
            ::free(rom);
            rom = nullptr;
            romSize = 0;
        }
        #endif
    }

    void marioToCheif() {
        auto playerPos = Engine::getPlayerPosition();
        if (playerPos.has_value()) {
            auto pos = playerPos.value();
            Vec3 marioWorldPos = Coordinates::haloToMario(pos);
            Vec3i targetChunk  = Coordinates::marioChunkForPosition(marioWorldPos);
            if (targetChunk.x != marioChunk.x ||
                targetChunk.y != marioChunk.y ||
                targetChunk.z != marioChunk.z) {
                MarioBSPChunk::reloadFor(targetChunk);
            }
            Vec3 local = Coordinates::marioWorldToLocal(marioWorldPos, marioChunk);
            sm64_set_mario_position(marioId, local.x, local.y, local.z);
            sm64_mario_heal(marioId, 0xFF);
        }
    }

    void dumpMarioGeometry();

    Vec3 marioWorldPositionMario() {
        Vec3 local = { marioState.position[0], marioState.position[1], marioState.position[2] };
        return Coordinates::marioLocalToWorld(local, marioChunk);
    }

    Vec3 marioWorldPosition() {
        return Coordinates::marioToHalo(marioWorldPositionMario());
    }

    Vec3 marioWorldVelocity() {
        return Coordinates::marioToHalo(Vec3{
            marioState.velocity[0],
            marioState.velocity[1],
            marioState.velocity[2]
        });
    }

    void update() {
        #ifdef ENABLE_MARIO
        std::lock_guard<std::mutex> updateLock(s_updateMutex);

        if (Engine::isPlayerInputDisabled()) {
            // Don't do anything with Mario if player input is disabled.
            MarioCamera::onDisable();
            deinitMario();
            return;
        }

        if (enableMario && marioId < 0) {
            initMario();
            return;
        }

        auto playerRec = Engine::getPlayerRecord();
        if (!playerRec) return;
        auto player = playerRec->entity();
        if (!player) return;

        updateGameSpeed(*player);
        updateShieldRegen(*player);
        Engine::Scripting::submit("object_set_scale (player0) 0.5 1");

        MarioCamera::onUpdate(marioWorldPosition());

        // if (GetAsyncKeyState(VK_F6) & 1) {
        //     dumpMarioGeometry();
        // }

        if (GetAsyncKeyState(VK_F3) & 1) enableMario = !enableMario;
        if (GetAsyncKeyState(VK_F4) & 1) {
            possessMario = !possessMario;
            if (possessMario) marioToCheif();
        }

        if (GetAsyncKeyState(VK_NUMPAD1) & 1) {
            sm64_set_mario_action_arg(marioId, ACT_RIDING_SHELL_GROUND, 0);
        }

        if (marioId < 0 || !enableMario) {
            MarioCamera::onDisable();
            return;
        }

        DynamicGeometry::update(marioState);

        if (marioInControl()) {
            // std::cout << "Mario in control this tick" << std::endl;
            Vec3 marioWorldPos = marioWorldPosition();
            player->pos = marioWorldPos;
            player->vel = marioWorldVelocity();
            Mario::updateInput(marioInputs, marioState, Engine::getPlayerCameraPointer());
        } else {
            // std::cout << "Chief in control this tick" << std::endl;
            MarioCamera::onDisable();
            // Clear input state.
            marioInputs = {}; 
            marioToCheif();
        }
        
        faceLookDirection(Engine::getPlayerCameraPointer()->fwd);

        if (marioInControl()) {
            std::lock_guard<std::mutex> sm64Lock(MarioAudio::sm64Mutex());
            sm64_mario_tick(marioId, &marioInputs, &marioState, &marioGeometry);
        }

        MarioBSPChunk::maintain();
        MarioAudio::update();

        updateMarioPose(marioGeometry);
        MarioAimingIK::apply();
        MarioMelee::tick();

        bool inForbiddenState = false
            || (marioState.action == ACT_START_SLEEPING)
            || (marioState.action == ACT_SLEEPING)
            || (marioState.action == ACT_WAKING_UP);
        if (inForbiddenState) sm64_set_mario_action(marioId, ACT_IDLE);

        // For now, always heal Mario. Cheif is responsible for recieving damage.
        sm64_mario_heal(marioId, 0xFF);

        #endif
    }

}

