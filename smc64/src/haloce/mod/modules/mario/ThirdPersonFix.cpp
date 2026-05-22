#include "../common.hpp"
#include "haloce/halo1/halo1.hpp"

namespace HaloCE::Mod::ThirdPersonFix {

    typedef void (*updateFlareTransform)(uint32_t flareHandle);
    updateFlareTransform originalUpdateFlareTransform = nullptr;
    void hkUpdateFlareTransform(uint32_t flareHandle) {
        UnloadLock lock;

        auto entry = Halo1::getFlareEntry(flareHandle);
        if (entry && entry->mountEntityHandle != NULL_HANDLE) {
            if (entry->mountEntityHandle == Halo1::getHeldWeaponHandle()) {
                return;
            }
        }

        originalUpdateFlareTransform(flareHandle);
    }

    typedef uint32_t (*spawnProjectile)(Halo1::ProjectileSpawnArgs* options, uint32_t flags);
    spawnProjectile originalSpawnProjectile = nullptr;
    uint32_t hkSpawnProjectile(Halo1::ProjectileSpawnArgs* options, uint32_t flags) {
        UnloadLock lock; // No unloading while we're still executing hook code.

        if (options->ownerEntityHandle != Halo1::getPlayerHandle()) {
            return originalSpawnProjectile(options, flags);
        }

        Vec3 spawnPosition = options->spawnPosition;
        auto camera = Halo1::getPlayerCameraPointer();
        if (camera && Memory::isAllocated(camera)) {
            float depth = (options->spawnPosition - camera->pos).dot( camera->fwd.normalize() );
            spawnPosition = camera->pos + camera->fwd.normalize() * depth;
        }

        options->spawnPosition = spawnPosition;
        uint32_t projectileHandle = originalSpawnProjectile(options, flags);

        auto projectile = Halo1::getEntityPointer(projectileHandle);
        if (projectile && Memory::isAllocated(projectile)) {
            if (camera && Memory::isAllocated(camera)) {
                // Move projectile spawn position to camera position
                projectile->pos = spawnPosition;

                // Adjust projectile velocity to match camera forward vector
                Vec3 vel = projectile->vel;
                Vec3 fwd = camera->fwd.normalize();
                float speed = vel.length();
                projectile->vel = fwd * speed;

                // Todo: Reimplement spread.
            }
        }

        return projectileHandle;
    }

    void init(uintptr_t halo1) {
        // Hook flare transform to suppress flares on player-held weapons (they render in the wrong place in third-person view)
        HOOK_FUNC( UpdateFlareTransform, 0xBE7D70U );
        // Hook projectile spawn function
        HOOK_FUNC( SpawnProjectile, 0xB35FD4U );
    }

    void free() {
        MH_RemoveHook( (void*) originalUpdateFlareTransform );
        MH_RemoveHook( (void*) originalSpawnProjectile );
    }

}
