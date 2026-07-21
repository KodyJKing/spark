#pragma once

#include "../EventBus.hpp"
#include "../SparkAPI.h"
#include "math/Vectors.hpp"

namespace Spark {
    // Single instance lives in Spark.dll (see TeleportPlayer.cpp) — exported so mod DLLs
    // share the same bus instead of getting their own disconnected copy.
    SPARK_API extern EventBus<void, Vec3> teleportPlayer;
}
