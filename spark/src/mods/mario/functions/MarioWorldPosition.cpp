#include "MarioWorldPosition.hpp"
#include "../MarioState.hpp"
#include "../Coordinates.hpp"

namespace Mod::Mario {

    Vec3 marioWorldPositionMario() {
        Vec3 local = { marioState.position[0], marioState.position[1], marioState.position[2] };
        return Coordinates::marioLocalToWorld(local, marioChunk);
    }

    Vec3 marioWorldPosition() {
        return Coordinates::marioToHalo(marioWorldPositionMario());
    }

    Vec3 marioWorldVelocity() {
        return Coordinates::marioToHalo(Vec3{
            marioState.velocity[0],
            marioState.velocity[1],
            marioState.velocity[2]
        });
    }

}
