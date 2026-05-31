#include "MarioState.hpp"
#include "math/Vectors.hpp"

namespace HaloCE::Mod::Mario {
    bool enableMario = true;
    bool possessMario = true;

    uint8_t* texture = nullptr;
    uint8_t* rom;
    size_t romSize = 0;

    int32_t marioId = -1;
    SM64MarioInputs marioInputs;
    SM64MarioState marioState;
    SM64MarioGeometryBuffers marioGeometry;

    Vec3i marioChunk = { 0, 0, 0 };
}
