#include "model_data.hpp" 
#include "engine/common.hpp"
#include <cstdint>

namespace Engine {
    // Chain: "halo1.dll"+0x02E5F788 , 0x00, 0x10, 0x00
    void* getModelDataPointer() {
        uintptr_t halo1 = Engine::dllBase();
        uintptr_t unk0 = halo1 + 0x02E5F788;
        uintptr_t unk1 = **reinterpret_cast<uintptr_t**>(unk0);
        uintptr_t unk2 = *reinterpret_cast<uintptr_t*>(unk1 + 0x10);
        return reinterpret_cast<void*>(unk2);
    }
}
