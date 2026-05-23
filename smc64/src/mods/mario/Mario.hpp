#pragma once

#include "libsm64.h"
#include "MarioModel.hpp"
#include "spark/mod/ModId.hpp"

namespace HaloCE::Mod::Mario {
    extern SM64MarioState marioState;
    void init(Spark::ModId modId);
    void free();
    void update();
    void debugRender();
}
