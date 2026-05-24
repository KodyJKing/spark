#include "MarioState.hpp"

namespace HaloCE::Mod::Mario {
    bool enableMario = true;
    bool possessMario = true;

    uint8_t* texture = nullptr;
    uint8_t* rom;
    size_t romSize = 0;

    SM64Surface* staticSurfaces = nullptr;
    size_t staticSurfacesCount = 0;

    int32_t marioId = -1;
    SM64MarioInputs marioInputs;
    SM64MarioState marioState;
    SM64MarioGeometryBuffers marioGeometry;
}
