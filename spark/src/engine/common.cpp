#include "common.hpp"

#include <Windows.h>

namespace Engine {
    uintptr_t dllBase() {
        return (uintptr_t) GetModuleHandleA( "halo1.dll" );
    }
}
