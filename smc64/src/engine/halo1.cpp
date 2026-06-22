
#include "halo1.hpp"
namespace Engine {
    bool isGameLoaded() { return dllBase() && isMapLoaded() && Engine::isCameraLoaded() && areEntitiesLoaded(); }
    float gameVolume() { return Memory::safeRead<float>( (uintptr_t) (dllBase() + 0x1C54490) ).value_or( 0.f ); }
}
