#include "common.hpp"

#include <Windows.h>

namespace Engine {

    static uintptr_t base = 0;

    void init() {
        base = (uintptr_t) GetModuleHandleA("halo1.dll");
    }

    uintptr_t dllBase() {
        if (!base) init();
        return base;
    }
}
