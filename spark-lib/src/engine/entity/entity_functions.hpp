#pragma once

#include "types.hpp"
#include "spark/SparkAPI.h"

namespace Engine {
    SPARK_API bool isReloading( Entity* entity );
    SPARK_API bool isDoingMelee( Entity* entity );
    SPARK_API bool isTransport( Entity* entity );
    SPARK_API bool isRidingTransport( Entity* entity );
    SPARK_API uint16_t boneCount(void * anim);
    SPARK_API bool entityValid(Entity *entity);
    SPARK_API bool entityValid(uint32_t entityHandle);
    SPARK_API bool entityHandleStale(uint32_t entityHandle);
}
