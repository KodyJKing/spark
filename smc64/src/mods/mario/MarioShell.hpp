#pragma once

#define SHIELD_RESOURCE_PATH "smc64\\jackal_shield\\jackal_shield"

namespace Mod::Mario::Shell {
    void updateShellState();
    bool updateShellPose(uint32_t entityHandle);
}
