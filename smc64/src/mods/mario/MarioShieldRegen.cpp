#include "MarioShieldRegen.hpp"

#include "MarioState.hpp"
#include "decomp/sm64.h"

namespace HaloCE::Mod::Mario {

    static uint32_t prevAction = 0;

    // Shield restored on the first frame of each acrobatic action.
    static constexpr float SIDE_FLIP_REGEN   = 0.2f;
    static constexpr float WALL_KICK_REGEN   = 0.3f;
    static constexpr float TRIPLE_JUMP_REGEN = 0.5f;

    void updateShieldRegen(Engine::Entity& player) {
        uint32_t action = marioState.action;
        bool risingEdge = (action != prevAction);
        prevAction = action;

        if (!risingEdge) return;

        float regen = 0.0f;
        switch (action) {
            case ACT_SIDE_FLIP:    regen = SIDE_FLIP_REGEN;   break;
            case ACT_WALL_KICK_AIR: regen = WALL_KICK_REGEN;  break;
            case ACT_TRIPLE_JUMP:  regen = TRIPLE_JUMP_REGEN; break;
            default: return;
        }

        bool hadOversheild = player.shield > 1.0f;
        player.shield += regen;
        if (!hadOversheild) {
            player.shield = min(player.shield, 1.0f);
        } else {
            player.shield = min(player.shield, 3.0f);
        }
    }

}
