#pragma once

#include "../common.hpp"
#include "types.hpp"
#include <functional>

namespace Engine {
    bool isFlareManagerLoaded();
    FlareManager* getFlareManager();
    FlareEntry* getFlareEntry( uint32_t flareHandle );
    void foreachFlareEntry( std::function<void( FlareEntry*, uint32_t handle )> cb );
}
