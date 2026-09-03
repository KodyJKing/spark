#pragma once

#include <stdint.h>
#include "spark/SparkAPI.h"

namespace Engine {
    SPARK_API void effectNewOnObjectMarker(uint32_t effectTagHandle, uint32_t entityHandle, const char* markerName);
}
