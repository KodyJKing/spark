#pragma once

#include <stdint.h>

#define NULL_HANDLE 0xFFFFFFFF

namespace Engine {
    void init();
    uintptr_t dllBase();
}
