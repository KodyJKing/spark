#pragma once

#include "libsm64.h"
#include "math/Vectors.hpp"
#include <cstdint>
#include <cstddef>

namespace HaloCE::Mod::Mario {
    extern bool enableMario;
    extern bool possessMario;

    extern uint8_t* texture;
    extern uint8_t* rom;
    extern size_t romSize;

    extern int32_t marioId;
    extern SM64MarioInputs marioInputs;
    extern SM64MarioState marioState;
    extern SM64MarioGeometryBuffers marioGeometry;

    // Chunk that marioState.position is expressed relative to.
    // True world mario pos = Coordinates::marioChunkOrigin(marioChunk) + marioState.position.
    extern Vec3i marioChunk;

    Vec3 getMarioPosition();
    void setMarioPosition(const Vec3& position);

    bool marioInControl();
}
