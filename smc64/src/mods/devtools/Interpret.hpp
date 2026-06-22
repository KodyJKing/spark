#pragma once
#include "engine/halo1.hpp"

namespace Engine {
    struct Interpretations {
        Tag* tag = nullptr;
        Entity* entity = nullptr;
        uintptr_t mapPointer = 0;
    };

    Interpretations interpretU32( uint32_t value );

    bool hasInterpretation(uint32_t value);
}
