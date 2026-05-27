#include "Mario.hpp"

#include "libsm64.h"
#include "decomp/sm64.h"
#include "decomp/surface_terrains.h"

#include <cstdlib>
#include <cstdio>
#include <cstdint>

#include <Windows.h>

#include <filesystem>
#include "engine/halo1.hpp"
#include "MarioInput.hpp"
#include "BSPConversion.hpp"
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
#include "FallDamageFix.hpp"
#include "MarioSkeleton.hpp"
#include "MarioModel.hpp"

namespace HaloCE::Mod::Mario {

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

    void initMario() {
        // Create a Mario instance at the origin.
        if (marioId < 0) {
            auto id = sm64_mario_create(99999.0f, 99999.0f, 99999.0f);
            auto playerPos = Engine::getPlayerPosition();
            if (playerPos.has_value()) {
                auto pos = playerPos.value();
                // Convert Halo CE coordinates to Super Mario 64 coordinates
                auto marioPos = Coordinates::haloToMario(pos);
                sm64_set_mario_position(id, marioPos.x, marioPos.y, marioPos.z);
            }
            marioId = id;
        }
        if (marioId < 0) {
            printf("Failed to create Mario instance.\n");
            return;
        }
        printf("Created Mario instance with ID: %d\n", marioId);

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
    }

    void initTestLevel() {
        // Load Halo CE static geometry and convert it to Super Mario 64 format
        auto surfaceVector = HaloCE::Mod::BSPConversion::haloGeometryToMario();

        staticSurfacesCount = surfaceVector.size();
        if (staticSurfacesCount == 0) {
            printf("No static surfaces found in Halo CE geometry.\n");
            return;
        }

        staticSurfaces = (SM64Surface*)malloc(sizeof(SM64Surface) * staticSurfacesCount);
        // memccpy(staticSurfaces, surfaceVector.data(), sizeof(SM64Surface), staticSurfacesCount);
        memcpy(staticSurfaces, surfaceVector.data(), sizeof(SM64Surface) * staticSurfacesCount);

        sm64_static_surfaces_load(staticSurfaces, staticSurfacesCount);
        printf("Loaded %zu static surfaces from Halo CE geometry.\n", staticSurfacesCount);
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

        sm64_register_debug_print_function(debugPrint);

        MarioAudio::init(rom);
        initTestLevel();
        initMario();

        ThirdPersonFix::registerHandlers(modId);
        MarioPickingFix::registerHandlers(modId);
        FallDamageFix::registerHandlers(modId);
        MarioCamera::registerHandlers(modId);
        #endif
    }

    void free() {
        #ifdef ENABLE_MARIO
        sm64_global_terminate();
        MarioAudio::free();

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
            // Convert Halo CE coordinates to Super Mario 64 coordinates
            auto marioPos = Coordinates::haloToMario(pos);
            sm64_set_mario_position(marioId, marioPos.x, marioPos.y, marioPos.z);
            sm64_mario_heal(marioId, 0xFF);
        }
    }

    void dumpMarioGeometry();

    Vec3 marioWorldPosition() {
        return Coordinates::marioToHalo(Vec3{
            marioState.position[0],
            marioState.position[1],
            marioState.position[2]
        });
    }


    void update() {
        #ifdef ENABLE_MARIO

        auto playerRec = Engine::getPlayerRecord();
        if (!playerRec) return;
        auto player = playerRec->entity();
        if (!player) return;

        updateGameSpeed(*player);
        updateShieldRegen(*player);
        Engine::Scripting::submit("object_set_scale (player0) 0.5 1");

        MarioCamera::onUpdate(marioWorldPosition());

        // F6 to dump Mario geometry buffers
        if (GetAsyncKeyState(VK_F6) & 1) {
            dumpMarioGeometry();
        }

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

        if (possessMario) {
            Vec3 marioWorldPos = marioWorldPosition();
            Vec3 difference = player->pos - marioWorldPos;
            float distance = difference.length();
            // If Cheif teleported, move Mario to Cheif
            if (distance > 5.0f) {
                marioToCheif();
            } else {
                player->pos = marioWorldPos;
            }
            Mario::updateInput(marioInputs, marioState, Engine::getPlayerCameraPointer());
        } else {
            MarioCamera::onDisable();
        }
        
        faceLookDirection(Engine::getPlayerCameraPointer()->fwd);

        {
            std::lock_guard<std::mutex> sm64Lock(MarioAudio::sm64Mutex());
            sm64_mario_tick(marioId, &marioInputs, &marioState, &marioGeometry);
        }
        sm64_set_mario_water_level(marioId, -999999.99f);
        MarioAudio::update();

        updateMarioPose(marioGeometry);

        bool inForbiddenState = false
            || (marioState.action == ACT_START_SLEEPING)
            || (marioState.action == ACT_SLEEPING)
            || (marioState.action == ACT_WAKING_UP);
        if (inForbiddenState) sm64_set_mario_action(marioId, ACT_IDLE);

        #endif
    }

}

