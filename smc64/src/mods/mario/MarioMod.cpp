#include "MarioMod.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/RenderBuses.hpp"
#include "Mario.hpp"
#include "engine/halo1.hpp"

void MarioMod::init() {
    using Bus = Spark::EventBus<void>;

    // Initialize Mario state first — sm64_global_init, geometry buffers, etc.
    // must all be complete before any hook can fire update().
    HaloCE::Mod::Mario::init(modId_);

    Spark::LoadCheckpoint::addHandler(modId_, +[](void*, auto next) {
        HaloCE::Mod::Mario::deinitMario();
        next();
    }, nullptr);

    Spark::UpdateAllEntities::addHandler(modId_, +[](void*, auto next) {
        HaloCE::Mod::Mario::update();
        next();
    }, nullptr);

    Spark::UpdateWorldBones::addHandler(modId_, +[](void*, auto next, uint32_t entityHandle) {
        auto rec = Engine::getEntityRecord(entityHandle);
        if (!rec) return next(entityHandle);
        auto entity = rec->entity();
        if (!entity) return next(entityHandle);
        next(entityHandle);
        HaloCE::Mod::Mario::MarioModel::processEntity(entityHandle, entity);
    }, nullptr);

    Spark::RenderEntity::addHandler(modId_, +[](void*, auto next, Engine::RenderEntityRequest* request) {
        next(request);
        HaloCE::Mod::Mario::MarioModel::renderEntity(request, Spark::RenderEntity::original);
    }, nullptr);

    Spark::onRenderDebugWorld.addHandler(modId_, +[](void*, Bus::Cursor next) {
        HaloCE::Mod::Mario::debugRender();
        next();
    }, nullptr);

}

void MarioMod::free() {
    HaloCE::Mod::Mario::free();
}
