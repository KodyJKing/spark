#define SPARK_EXPORTS
#include "RenderBuses.hpp"

// Single, exported definitions for the render buses declared in RenderBuses.hpp.
// These hold shared mutable state (handler lists, mutex) that must have exactly one
// instance shared by Spark and every mod DLL — see the comment in RenderBuses.hpp.

namespace Spark {
    EventBus<void> onRenderPauseMenuTabs;
    EventBus<void> onRenderDebugUI;
    EventBus<void> onRenderDebugWorld;
}
