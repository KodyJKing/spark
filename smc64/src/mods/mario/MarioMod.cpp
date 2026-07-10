#include "MarioMod.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/RenderBuses.hpp"
#include "Mario.hpp"
#include "engine/halo1.hpp"
#include "level-edit/MarioLevelEdit.hpp"
#include "MarioChiefPose.hpp"
#include "MarioWeaponPose.hpp"
#include "MarioPauseTab.hpp"
#include "spark/events/TeleportPlayer.hpp"
#include "functions/TeleportMario.hpp"

void MarioMod::init() {
    using Bus = Spark::EventBus<void>;

    // Initialize Mario state first — sm64_global_init, geometry buffers, etc.
    // must all be complete before any hook can fire update().
    Mod::Mario::init(modId_);

    // Initialize Mario model handlers.
    Mod::Mario::MarioModel::addHandlers(modId_);
    Mod::Mario::MarioWeaponPose::addHandlers(modId_);

    Spark::LoadCheckpoint::addHandler(modId_, +[](void*, auto next) {
        Mod::Mario::deinitMario();
        next();
    }, nullptr);

    Spark::UpdateAllEntities::addHandler(modId_, +[](void*, auto next) {
        Mod::Mario::update();
        next();
    }, nullptr);

    Spark::RenderEntity::addHandler(modId_, +[](void*, auto next, Engine::RenderEntityRequest* request) {
        bool skip = false;
        // if (request && Engine::entityValid(request->entityHandle)) {
        //     auto entity = Engine::getEntityPointer(request->entityHandle);
        //     std::string tagPath = entity ? entity->getTagResourcePath() : "";
        //     // If it contains "cyborg", skip rendering this entity.
        //     if (tagPath.find("cyborg") != std::string::npos) {
        //         if (tagPath.find("_unarmed") == std::string::npos) {
        //             skip = true;
        //         }
        //     }
        // }

        if (request && Engine::entityValid(request->entityHandle)) {
            auto entity = Engine::getEntityPointer(request->entityHandle);
            std::string tagPath = entity ? entity->getTagResourcePath() : "";
            if (tagPath.find("cyborg") != std::string::npos) {
                Spark::ObjectSetScale::original(request->entityHandle, 0.5f, 0);
                if (entity->health < 0.0f) skip = true;
            }
        }

        if (!skip) {
            next(request);
        }
        Mod::Mario::MarioModel::renderEntity(request, Spark::RenderEntity::original);
    }, nullptr);

    Spark::onRenderDebugWorld.addHandler(modId_, +[](void*, Bus::Cursor next) {
        Mod::Mario::debugRender();
        next();
    }, nullptr);

    Spark::onRenderPauseMenuTabs.addHandler(modId_, +[](void*, Bus::Cursor next) {
        Mod::Mario::renderPauseMenuTab();
        next();
    }, nullptr);

    Mod::Mario::LevelEdit::initHandlers(modId_);
    Mod::Mario::MarioChiefPose::initHandlers(modId_);

    // Teleport player handler:
    Spark::teleportPlayer.addHandler(modId_, +[](void*, Vec3 position) {
        Mod::Mario::teleportMario(position);
    });

    // Spark::RenderPassenger::addHandler(modId_, +[](void*, auto next, uint64_t param_1, uint16_t* param_2, uint32_t entityHandle) {
    //     if (entityHandle != Engine::getPlayerHandle()) {
    //         next(param_1, param_2, entityHandle);
    //     }
    // }, nullptr);

}

void MarioMod::free() {
    Mod::Mario::free();
}
