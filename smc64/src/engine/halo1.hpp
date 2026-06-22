// Reverse engineered Halo 1 structures and access functions go here.

#pragma once

#include "common.hpp"
#include "math.hpp"
#include "tag.hpp"
#include "entity/index.hpp"
#include "flares/index.hpp"
#include "camera.hpp"
#include "player.hpp"
#include "map.hpp"
#include "bsp/index.hpp"
#include "tags/index.hpp"
#include "types/index.hpp"
#include "raycast.hpp"
#include "scripting/Scripting.hpp"
#include "scripting/TerminalOutput.hpp"

namespace Engine {
    bool isCameraLoaded();
    bool isGameLoaded();
}
