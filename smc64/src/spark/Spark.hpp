#pragma once

namespace Spark {
    // When true, the debug overlay (ESP window, world-space draws) is rendered.
    inline bool showDebugOverlay = false;

    void modThreadUpdate();
    void free();

} // namespace Spark
