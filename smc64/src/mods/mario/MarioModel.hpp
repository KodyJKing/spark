#pragma once

#include "spark/mod/ModId.hpp"
#include "engine/halo1.hpp"
#include "stdint.h"

namespace Mod::Mario::MarioModel {
    void renderEntity(Engine::RenderEntityRequest* request, Engine::renderEntity_t renderEntityOriginal);

    void addHandlers(Spark::ModId modId);

    extern uint32_t marioHandle;
    
    bool isMario(uint32_t entityHandle);
    uint32_t playerWeaponHandle();
    uint32_t spawnMarioModel(Vec3 position);
}
