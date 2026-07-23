#include "math/Vectors.hpp"
#include "engine/math.hpp"

// This is a test/example. Generally, functions this small do not need a decomp because we don't care to modify their behavior and are already confident we understand them.
Vec3* transformVec(Engine::Transform *tr,Vec3 *vecIn,Vec3 *vecOut) {
  Vec3 vecInScaled = (tr->w != 1.0) ? (*vecIn * tr->w) : *vecIn;
  *vecOut = tr->x * vecInScaled.x + tr->y * vecInScaled.y + tr->z * vecInScaled.z;
  return vecOut;
}
