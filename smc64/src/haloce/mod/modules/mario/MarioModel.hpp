#pragma once

#include "haloce/halo1/halo1.hpp"
#include "stdint.h"

namespace HaloCE::Mod::Mario::MarioModel {
    void processEntity(uint32_t entityHandle, Halo1::Entity* entity);

    void renderEntity(Halo1::RenderEntityRequest* request, Halo1::renderEntity_t renderEntityOriginal);

    extern uint32_t marioHandle;
    
    bool isMario(uint32_t entityHandle);
}
