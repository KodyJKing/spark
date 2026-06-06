#include "MarioState.hpp"
#include "math/Vectors.hpp"
#include "engine/halo1.hpp"
#include "decomp/sm64.h"

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

    // Mario's position in Mario coordinates.
    Vec3 getMarioPosition() {
        return {
            marioState.position[0],
            marioState.position[1],
            marioState.position[2]
        };
    }

    void setMarioPosition(const Vec3& position) {
        sm64_set_mario_position(marioId, position.x, position.y, position.z);
    }

    bool marioInControl() {
        auto playerEntity = Engine::getPlayerEntity();
        if (!playerEntity) return false;

        if (playerEntity->vehicleHandle != NULL_HANDLE) return false;
        if (playerEntity->parentHandle != NULL_HANDLE) return false;

        return enableMario && possessMario;
    }

    bool marioAirborne() {
        return (marioState.action & ACT_FLAG_AIR) != 0;
    }
}
