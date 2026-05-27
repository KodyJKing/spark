#include "MarioGameSpeed.hpp"

#include "MarioState.hpp"
#include "engine/halo1.hpp"
#include "decomp/sm64.h"

#include <cstdio>
#include <Windows.h>

namespace HaloCE::Mod::Mario {

    float gamespeed = 1.0f;

    void setGameSpeed(float speed) {
        if (gamespeed == speed) return;
        gamespeed = speed;
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "game_speed_value %.2f", speed);
        Engine::Scripting::submit(buffer);
    }

    void updateGameSpeed(Engine::Entity& player) {
        bool airborne = (marioState.action & ACT_FLAG_AIR) != 0;
        bool hasSheilds = player.shield > 0;
        bool canSlowdown = airborne && hasSheilds;
        bool slowDown = canSlowdown && (GetAsyncKeyState(VK_CONTROL) & 0x8000);
        if (slowDown) {
            player.shield -= 0.05f;
            setGameSpeed(0.25f);
        } else {
            setGameSpeed(1.0f);
        }
    }

}
