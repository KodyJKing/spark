#pragma once

#include "libsm64.h"
#include "MarioModel.hpp"
#include "mod/ModId.hpp"

namespace HaloCE::Mod::Mario {
    extern SM64MarioState marioState;
    void init(ModId modId);
    void free();
    void update();
    void debugRender();
}
