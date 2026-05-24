#pragma once

#include "libsm64.h"
#include <cstdint>
#include <cstddef>

namespace HaloCE::Mod::Mario {
    extern bool enableMario;
    extern bool possessMario;

    extern uint8_t* texture;
    extern uint8_t* rom;
    extern size_t romSize;

    extern SM64Surface* staticSurfaces;
    extern size_t staticSurfacesCount;

    extern int32_t marioId;
    extern SM64MarioInputs marioInputs;
    extern SM64MarioState marioState;
    extern SM64MarioGeometryBuffers marioGeometry;
}
