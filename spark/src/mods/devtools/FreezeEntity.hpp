#pragma once

#include "engine/halo1.hpp"
#include "spark/mod/ModId.hpp"

namespace Mod::DevTools::FreezeEntity {
    void init(Spark::ModId modId);
    
    bool& entityFrozen(Engine::Entity* entity);
    bool& categoryFrozen(Engine::EntityCategory category);
    bool& tagHandleFrozen(uint32_t tagHandle);

} // namespace Mod::DevTools
