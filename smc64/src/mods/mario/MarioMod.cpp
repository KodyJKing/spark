#include "MarioMod.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/RenderBuses.hpp"
#include "Mario.hpp"
#include "engine/halo1.hpp"
#include "level-edit/MarioLevelEdit.hpp"
#include "spark/events/TeleportPlayer.hpp"
#include "functions/TeleportMario.hpp"

void MarioMod::init() {
    using Bus = Spark::EventBus<void>;

    // Initialize Mario state first — sm64_global_init, geometry buffers, etc.
    // must all be complete before any hook can fire update().
    HaloCE::Mod::Mario::init(modId_);

    // Initialize Mario model handlers.
    HaloCE::Mod::Mario::MarioModel::addHandlers(modId_);

    Spark::LoadCheckpoint::addHandler(modId_, +[](void*, auto next) {
        HaloCE::Mod::Mario::deinitMario();
        next();
    }, nullptr);

    Spark::UpdateAllEntities::addHandler(modId_, +[](void*, auto next) {
        HaloCE::Mod::Mario::update();
        next();
    }, nullptr);

    Spark::RenderEntity::addHandler(modId_, +[](void*, auto next, Engine::RenderEntityRequest* request) {
        next(request);
        HaloCE::Mod::Mario::MarioModel::renderEntity(request, Spark::RenderEntity::original);
    }, nullptr);

    Spark::onRenderDebugWorld.addHandler(modId_, +[](void*, Bus::Cursor next) {
        HaloCE::Mod::Mario::debugRender();
        next();
    }, nullptr);

    Mod::Mario::LevelEdit::initHandlers(modId_);

    // Teleport player handler:
    Spark::teleportPlayer.addHandler(modId_, +[](void*, Vec3 position) {
        HaloCE::Mod::Mario::teleportMario(position);
    });


}

void MarioMod::free() {
    HaloCE::Mod::Mario::free();
}
