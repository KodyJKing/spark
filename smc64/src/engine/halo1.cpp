
#include "halo1.hpp"
namespace Engine {
    bool isGameLoaded() { return dllBase() && isMapLoaded() && Engine::isCameraLoaded() && areEntitiesLoaded(); }
}
