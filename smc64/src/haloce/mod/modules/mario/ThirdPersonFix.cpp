#include "../common.hpp"
#include "haloce/halo1/halo1.hpp"
#include "hook/Hooks.hpp"

namespace HaloCE::Mod::ThirdPersonFix {

    void registerHandlers() {
        // Suppress flares on player-held weapons (they render in the wrong place in third-person view)
        UpdateFlareTransform::addHandler([](UpdateFlareTransform::Next next, uint32_t flareHandle) {
            auto entry = Halo1::getFlareEntry(flareHandle);
            if (entry && entry->mountEntityHandle != NULL_HANDLE) {
                if (entry->mountEntityHandle == Halo1::getHeldWeaponHandle()) {
                    return;
                }
            }
            next(flareHandle);
        });

        SpawnProjectile::addHandler([](SpawnProjectile::Next next, Halo1::ProjectileSpawnArgs* options, uint32_t flags) -> uint32_t {
            if (options->ownerEntityHandle != Halo1::getPlayerHandle()) {
                return next(options, flags);
            }

            Vec3 spawnPosition = options->spawnPosition;
            auto camera = Halo1::getPlayerCameraPointer();
            if (camera && Memory::isAllocated(camera)) {
                float depth = (options->spawnPosition - camera->pos).dot(camera->fwd.normalize());
                spawnPosition = camera->pos + camera->fwd.normalize() * depth;
            }

            options->spawnPosition = spawnPosition;
            uint32_t projectileHandle = next(options, flags);

            auto projectile = Halo1::getEntityPointer(projectileHandle);
            if (projectile && Memory::isAllocated(projectile)) {
                if (camera && Memory::isAllocated(camera)) {
                    projectile->pos = spawnPosition;

                    Vec3 fwd = camera->fwd.normalize();
                    float speed = projectile->vel.length();
                    projectile->vel = fwd * speed;

                    // Todo: Reimplement spread.
                }
            }

            return projectileHandle;
        });
    }

}
