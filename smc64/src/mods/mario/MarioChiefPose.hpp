#pragma once

#include "spark/mod/ModId.hpp"

namespace Mod::Mario::MarioChiefPose {
    void initHandlers(Spark::ModId modId);
    void updatePose();
    void render();
}
