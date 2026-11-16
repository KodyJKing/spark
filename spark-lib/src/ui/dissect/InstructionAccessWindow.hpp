#pragma once

#include <cstddef>

namespace SparkLib::UI::DissectTag {
#ifndef SPOOL
    struct State;
    struct WindowState;

    void renderInstructionWatchButton(State& state, WindowState& window, void* tagData, size_t size);
    void renderInstructionAccessWindow(WindowState& window, bool interactive = true);
    void renderInstructionAccessWindows(State& state, bool interactive);
#endif
}
