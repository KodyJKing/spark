#pragma once

#include "types.hpp"

namespace Engine {
    bool isReloading( Entity* entity );
    bool isDoingMelee( Entity* entity );
    bool isTransport( Entity* entity );
    bool isRidingTransport( Entity* entity );
    uint16_t boneCount(void * anim);
    bool entityValid(Entity *entity);
    bool entityValid(uint32_t entityHandle);
}
