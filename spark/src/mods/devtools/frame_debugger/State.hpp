#pragma once

#include <cstdint>

namespace Mod::DevTools::FrameDebugger {

    struct State {
        uint64_t frameCount;
        uint64_t drawsThisFrame;
        uint64_t drawsLastFrame;
        void* finalCallReturnAddress;
        uint64_t drawsToAllow = 200;
        bool windowOpen = false;
        bool debug = false;

        uint64_t stopAtCall();

        bool enabled();
        void setEnabled(bool value);

        void deinit();
    };

    extern State state;

}
