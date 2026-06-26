#include "CheifToMario.hpp"

#include "../Mario.hpp"

namespace Mod::Mario {

    void cheifToMario(Engine::Entity* player) {
        Vec3 marioWorldPos = marioWorldPosition();
        player->pos = marioWorldPos;
        player->vel = marioWorldVelocity();
    }

}
