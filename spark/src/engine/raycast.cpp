#include "raycast.hpp"
#include "common.hpp"
#include "engine/player.hpp"

namespace Engine {

    typedef void (*RaycastFunctionType)(uint64_t flags, Vec3 *origin, Vec3 *displacement, uint32_t sourceEntityHandle, RaycastResult *raycastResultOut);

    void raycast(uint64_t flags, Vec3 *origin, Vec3 *displacement, uint32_t sourceEntityHandle, RaycastResult *raycastResultOut) {
        uintptr_t raycastAddress = dllBase() + 0xB913FC; 
        RaycastFunctionType raycastFunc = (RaycastFunctionType)raycastAddress;
        raycastFunc(flags, origin, displacement, sourceEntityHandle, raycastResultOut);
    }

    void raycastPlayerCrosshair(RaycastResult *raycastResultOut, float maxDistance, uint64_t flags) {
        auto player = Engine::getPlayerEntity();
        if (!player) return;
        auto camera = Engine::getPlayerCameraPointer();
        if (!camera) return;

        raycastResultOut->hitType = HitType_Nothing;

        Vec3 origin = camera->pos;
        Vec3 forward = camera->fwd;
        Vec3 displacement = forward * maxDistance;

        raycast(flags, &origin, &displacement, Engine::getPlayerHandle(), raycastResultOut);
    }

}
