#pragma once
#include "spark/SparkAPI.h"

namespace Spark {
    // When true, the debug overlay (ESP window, world-space draws) is rendered.
    // Shared mutable state across the Spark <-> mod DLL boundary — must be exactly one
    // instance (see RenderBuses.hpp for the same pattern/rationale). Defined once in
    // Spark.cpp; an `inline` variable here would give each mod DLL its own private,
    // permanently-false copy instead of Spark's real one.
    SPARK_API extern bool showDebugOverlay;

    void modThreadUpdate();
    void free();

} // namespace Spark
