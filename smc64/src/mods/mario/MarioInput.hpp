#pragma once
#include "libsm64.h"
#include "engine/halo1.hpp"

namespace HaloCE::Mod::Mario {
    void updateInput(SM64MarioInputs& inputs, SM64MarioState& marioState, Engine::Camera* camera = nullptr);
}
