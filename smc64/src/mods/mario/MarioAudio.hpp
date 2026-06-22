#pragma once

#include <cstdint>
#include <mutex>

namespace HaloCE::Mod::Mario::MarioAudio {
    void init(const uint8_t* rom);
    void update();
    void free();

    // Acquire this mutex around sm64_mario_tick to prevent concurrent audio synthesis.
    // Both sm64_mario_tick and sm64_audio_tick share SM64 global state.
    std::mutex& sm64Mutex();

    // Adjust XAudio2 playback rate to match game speed (0.25 – 1.0).
    // Audio pitch shifts with speed — classic slow-motion effect.
    void setGameSpeed(float speed);
}
