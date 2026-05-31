#include "engine/halo1.hpp"
#include "spark/hook/Hooks.hpp"
#include "MarioSkeleton.hpp"
namespace HaloCE::Mod::ThirdPersonFix {

    void registerHandlers(Spark::ModId modId) {
        // Suppress flares on player-held weapons (they render in the wrong place in third-person view)
        Spark::UpdateFlareTransform::addHandler(modId, +[](void* /*ctx*/, auto next, uint32_t flareHandle) {
            auto entry = Engine::getFlareEntry(flareHandle);
            if (entry && entry->mountEntityHandle != NULL_HANDLE) {
                if (entry->mountEntityHandle == Engine::getHeldWeaponHandle()) {
                    return;
                }
            }
            next(flareHandle);
        }, nullptr);

        Spark::SpawnProjectile::addHandler(modId, +[](void* /*ctx*/, auto next, Engine::ProjectileSpawnArgs* options, uint32_t flags) -> uint32_t {
            if (options->ownerEntityHandle != Engine::getPlayerHandle()) {
                return next(options, flags);
            }

            // Correct spawn position.
            Vec3 spawnPosition = options->spawnPosition;
            auto camera = Engine::getPlayerCameraPointer();
            if (camera && Memory::isAllocated(camera)) {
                
                // Reference point for choosing depth along look axis.
                Vec3 depthReference = options->spawnPosition;
                auto leftHand = Mario::getMarioBonePointerByName("left_hand");
                if (leftHand && Memory::isAllocated(leftHand)) {
                    depthReference = leftHand->pos;
                }

                
                float depth = (depthReference - camera->pos).dot(camera->fwd.normalize());
                spawnPosition = camera->pos + camera->fwd.normalize() * depth;
            }

            options->spawnPosition = spawnPosition;
            uint32_t projectileHandle = next(options, flags);

            // Correct direction.
            auto projectile = Engine::getEntityPointer(projectileHandle);
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
        }, nullptr);
    }

}
