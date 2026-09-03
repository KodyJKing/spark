#include "effects.hpp"
#include "common.hpp"

namespace Engine {
    void effectNewOnObjectMarker(uint32_t effectTagHandle, uint32_t entityHandle, const char* markerName) {
        uintptr_t fnAddress = dllBase() + 0xC581AC;
        using FnType = void(*)(uint32_t, uint32_t, const char*);
        FnType fn = reinterpret_cast<FnType>(fnAddress);
        fn(effectTagHandle, entityHandle, markerName);
    }
}
