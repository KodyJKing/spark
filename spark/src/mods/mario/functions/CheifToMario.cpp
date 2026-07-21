#include "CheifToMario.hpp"

#include "../MarioState.hpp"
#include "../Mario.hpp"

// #define DEBUG_CHIEF_TO_MARIO 1

#if DEBUG_CHIEF_TO_MARIO
#include <iostream>
#define LOG(msg) std::cout << "[CheifToMario] " << msg << std::endl;
#else
#define LOG(MSGBOXPARAMS)
#endif

namespace Mod::Mario {

    void cheifToMario(Engine::Entity* player) {
        if (marioTickCount == 0) {
            LOG("Mario has not ticked yet. Skipping cheifToMario.");
            return;
        }
        
        Vec3 marioWorldPos = marioWorldPosition();
        Vec3 diff = marioWorldPos - player->pos;
        if (diff.length() > 1.0f) {
            LOG("Significant position difference detected: " << diff.length());
            LOG("Player position: " << player->pos.x << ", " << player->pos.y << ", " << player->pos.z);
            LOG("Mario position: " << marioWorldPos.x << ", " << marioWorldPos.y << ", " << marioWorldPos.z);
        }

        player->pos = marioWorldPos;
        player->vel = marioWorldVelocity();
    }

}
