#pragma once

#include <string>
#include <vector>

namespace Engine::TerminalOutput {

    struct Entry {
        std::string text;
        float color[4]; // RGBA floats [0,1]
    };

    // Returns any new terminal entries since the last call, oldest first.
    // Call each frame; the first call initialises the cursor (no replay of history).
    std::vector<Entry> poll();

} // namespace Engine::TerminalOutput
