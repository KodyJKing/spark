#include "MarioGameSpeed.hpp"

#include "MarioAudio.hpp"
#include "MarioState.hpp"
#include "engine/halo1.hpp"
#include "decomp/sm64.h"

#include <cstdio>
#include <Windows.h>

namespace HaloCE::Mod::Mario {

    float smoothedGameSpeed = 1.0f;
    float gamespeed = 1.0f;

    void setGameSpeed(float speed) {
        // if (gamespeed == speed) return;
        gamespeed = speed;
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "game_speed_value %.2f", speed);
        Engine::Scripting::submit(buffer);
        MarioAudio::setGameSpeed(speed);
    }


    void updateGameSpeed(Engine::Entity& player) {
        bool airborne = (marioState.action & ACT_FLAG_AIR) != 0;
        bool hasSheilds = player.shield > 0;
        bool canSlowdown = airborne && hasSheilds;
        bool slowDown = canSlowdown && (GetAsyncKeyState(VK_CONTROL) & 0x8000);
        if (slowDown) {
            player.shield -= 0.05f;
            gamespeed = 0.25f;
        } else {
            gamespeed = 1.0f;
        }

        // Smoothly interpolate the game speed to avoid abrupt changes.
        float smoothingFactor = 0.4f; // Adjust this for faster/slower smoothing
        smoothedGameSpeed += (gamespeed - smoothedGameSpeed) * smoothingFactor;

        setGameSpeed(smoothedGameSpeed);
    }

}
