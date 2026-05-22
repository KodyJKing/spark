#pragma once

#include "engine/halo1.hpp"
#include "stdint.h"

namespace HaloCE::Mod::Mario::MarioModel {
    void processEntity(uint32_t entityHandle, Engine::Entity* entity);

    void renderEntity(Engine::RenderEntityRequest* request, Engine::renderEntity_t renderEntityOriginal);

    extern uint32_t marioHandle;
    
    bool isMario(uint32_t entityHandle);
}
