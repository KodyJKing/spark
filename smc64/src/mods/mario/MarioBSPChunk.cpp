#include "MarioBSPChunk.hpp"

#include "libsm64.h"

#include "MarioState.hpp"
#include "Coordinates.hpp"
#include "BSPConversion.hpp"
#include "DynamicGeometry.hpp"

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>

// #define DEBUG_BSP_CHUNK 1

#ifdef DEBUG_BSP_CHUNK
    #include <iostream>
    #define LOG(x) std::cout << "[MarioBSPChunk] " << x << std::endl;
#else
    #define LOG(x) ;
#endif

namespace HaloCE::Mod::Mario::MarioBSPChunk {

    // Mario's chunk is the frame all of sm64's coords (static surfaces, dynamic objects,
    // marioState.position) are expressed in. We allow Mario's local position to drift up
    // to chunkHalfExtent + reloadMargin from the chunk origin before performing an
    // atomic recenter + static reload.
    float reloadMargin = 4096.0f;

    static SM64Surface* staticSurfaces    = nullptr;
    static size_t       staticSurfacesCount = 0;

    static uint64_t lastBSPSignature = 0;

    Vec3i getLoadedChunk() { return marioChunk; }

    static void uploadFor(Vec3i chunk) {
        auto surfaces = HaloCE::Mod::BSPConversion::haloGeometryToMarioForChunk(chunk, 1);

        if (staticSurfaces) {
            ::free(staticSurfaces);
            staticSurfaces = nullptr;
            staticSurfacesCount = 0;
        }

        staticSurfacesCount = surfaces.size();
        if (staticSurfacesCount == 0) {
            LOG("No static surfaces for chunk (" << chunk.x << ", " << chunk.y << ", " << chunk.z << ")");
            sm64_static_surfaces_load(nullptr, 0);
            return;
        }

        staticSurfaces = (SM64Surface*) malloc(sizeof(SM64Surface) * staticSurfacesCount);
        memcpy(staticSurfaces, surfaces.data(), sizeof(SM64Surface) * staticSurfacesCount);

        sm64_static_surfaces_load(staticSurfaces, (uint32_t) staticSurfacesCount);
        LOG("Loaded " << staticSurfacesCount << " surfaces for chunk (" << chunk.x << ", " << chunk.y << ", " << chunk.z << ")");

        lastBSPSignature = Engine::getBSPSignature();
    }

    void init(Vec3i initialChunk) {
        marioChunk = initialChunk;
        uploadFor(marioChunk);
    }

    void reloadFor(Vec3i newChunk) {
        Vec3i oldChunk = marioChunk;
        marioChunk     = newChunk;
        uploadFor(marioChunk);
        HaloCE::Mod::Mario::DynamicGeometry::onLoadedChunkChanged(oldChunk, newChunk);
    }

    void checkMovement() {
        if (marioId < 0) return;

        float ext       = Coordinates::chunkExtent;
        float half      = Coordinates::chunkHalfExtent();
        float threshold = half + reloadMargin;

        float* p = marioState.position;
        Vec3i delta = { 0, 0, 0 };
        if (std::abs(p[0]) > threshold) delta.x = (int32_t) std::lround(p[0] / ext);
        if (std::abs(p[1]) > threshold) delta.y = (int32_t) std::lround(p[1] / ext);
        if (std::abs(p[2]) > threshold) delta.z = (int32_t) std::lround(p[2] / ext);
        if (delta.x == 0 && delta.y == 0 && delta.z == 0) return;

        // Atomic recenter: shift Mario's sm64 coord and reload static + dynamic geometry
        // for the new chunk, so the static collision world stays beneath Mario.
        Vec3i newChunk = { marioChunk.x + delta.x, marioChunk.y + delta.y, marioChunk.z + delta.z };
        p[0] -= (float) delta.x * ext;
        p[1] -= (float) delta.y * ext;
        p[2] -= (float) delta.z * ext;
        sm64_set_mario_position(marioId, p[0], p[1], p[2]);

        LOG("Mario moved to chunk (" << newChunk.x << ", " << newChunk.y << ", " << newChunk.z
            << ") with local position (" << p[0] << ", " << p[1] << ", " << p[2] << ")");
        reloadFor(newChunk);
    }

    void checkSignature() {
        uint64_t currentBSPSignature = Engine::getBSPSignature();
        if (currentBSPSignature != lastBSPSignature) {
            LOG("BSP signature changed, reloading Mario's static surfaces for chunk ("
                << marioChunk.x << ", " << marioChunk.y << ", " << marioChunk.z << ")");
            reloadFor(marioChunk);
        }
    }

    void maintain() {
        checkMovement();
        checkSignature();
    }

    void free() {
        if (staticSurfaces) {
            ::free(staticSurfaces);
            staticSurfaces = nullptr;
            staticSurfacesCount = 0;
        }
    }

}
