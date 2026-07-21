#pragma once
#include "spark/EventBus.hpp"
#include "spark/Spark.hpp"
#include "spark/SparkAPI.h"

namespace Spark {

    // These buses hold shared mutable state (handler lists, mutex) that must have exactly
    // one instance across all modules. Defined once in RenderBuses.cpp and exported —
    // a mod DLL that just included an `inline` variable here would get its own private,
    // disconnected copy instead of Spark's real one.

    // Fires inside the pause menu tab bar (BeginTabBar/EndTabBar owned by Overlay).
    // Handlers render BeginTabItem/EndTabItem pairs.
    SPARK_API extern EventBus<void> onRenderPauseMenuTabs;

    // Fires each frame when the game overlay is active.
    // Handlers open ImGui windows.
    SPARK_API extern EventBus<void> onRenderDebugUI;

    // Fires inside the ESP world-space draw window.
    // Spark syncs Spark::Overlay::ESP::camera to the player camera before dispatch.
    // Handlers use Spark::Overlay::ESP draw calls.
    SPARK_API extern EventBus<void> onRenderDebugWorld;

    // Terminal for void buses — does nothing.
    inline void noopTerminal(void*) {}

}
