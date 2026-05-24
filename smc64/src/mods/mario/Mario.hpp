#pragma once

#include "MarioState.hpp"
#include "MarioModel.hpp"
#include "spark/mod/ModId.hpp"

namespace HaloCE::Mod::Mario {
    void init(Spark::ModId modId);
    void free();
    void update();
    void debugRender();
}
