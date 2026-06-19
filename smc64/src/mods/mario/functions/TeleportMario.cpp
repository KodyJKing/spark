#include "../Coordinates.hpp"
#include "../MarioState.hpp"

namespace HaloCE::Mod::Mario {

    void teleportMario(const Vec3& haloPosition) {
        auto localMarioPos = Coordinates::haloToMarioLocal(haloPosition, marioChunk);
        setMarioPosition(localMarioPos);
    }

}