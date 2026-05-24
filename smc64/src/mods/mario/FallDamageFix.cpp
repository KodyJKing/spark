#include "FallDamageFix.hpp"
#include "spark/hook/Hooks.hpp"
#include <string>

namespace HaloCE::Mod::Mario::FallDamageFix {

    void registerHandlers(Spark::ModId modId) {
        Spark::DamageEntity::addHandler(modId, +[](void* /*ctx*/, auto next, Engine::DamageEvent* event, uint32_t entityHandle, uint16_t p2, uint16_t p3, int16_t hitBoneIndex, uint64_t p5) {

            // Ignore fall damage to the player.
            // Todo: Check enableMario && possessMario.
            if (entityHandle == Engine::getPlayerHandle()) {
                auto damageTypeTagId = event->damageTypeTagHandle;
                auto damageTag = Engine::getTag(damageTypeTagId);
                
                std::string path = damageTag ? damageTag->getResourcePath() : "";
                if (path.find("globals/falling") != std::string::npos)
                    return;
            }
            
            next(event, entityHandle, p2, p3, hitBoneIndex, p5);
        }, nullptr);
    }

}
