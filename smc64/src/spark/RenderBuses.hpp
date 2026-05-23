#pragma once
#include "spark/EventBus.hpp"
#include "spark/Spark.hpp"

namespace Spark {

    // Fires inside the pause menu tab bar (BeginTabBar/EndTabBar owned by Overlay).
    // Handlers render BeginTabItem/EndTabItem pairs.
    inline EventBus<void> onRenderPauseMenuTabs;

    // Fires each frame when the game overlay is active.
    // Handlers open ImGui windows.
    inline EventBus<void> onRenderDebugUI;

    // Fires inside the ESP world-space draw window.
    // Spark syncs Overlay::ESP::camera to the player camera before dispatch.
    // Handlers use Overlay::ESP draw calls.
    inline EventBus<void> onRenderDebugWorld;

    // Terminal for void buses — does nothing.
    inline void noopTerminal(void*) {}

}
