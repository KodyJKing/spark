#include "MarioMod.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/RenderBuses.hpp"
#include "Mario.hpp"
#include "engine/halo1.hpp"

void MarioMod::init() {
    using Bus = Spark::EventBus<void>;

    Spark::UpdateAllEntities::addHandler(modId_, +[](void*, Spark::UpdateAllEntities::Cursor next) {
        HaloCE::Mod::Mario::update();
        next();
    }, nullptr);

    Spark::UpdateWorldBones::addHandler(modId_, +[](void*, Spark::UpdateWorldBones::Cursor next, uint32_t entityHandle) {
        auto rec = Engine::getEntityRecord(entityHandle);
        if (!rec) return next(entityHandle);
        auto entity = rec->entity();
        if (!entity) return next(entityHandle);
        next(entityHandle);
        HaloCE::Mod::Mario::MarioModel::processEntity(entityHandle, entity);
    }, nullptr);

    Spark::RenderEntity::addHandler(modId_, +[](void*, Spark::RenderEntity::Cursor next, Engine::RenderEntityRequest* request) {
        next(request);
        HaloCE::Mod::Mario::MarioModel::renderEntity(request, Spark::RenderEntity::original);
    }, nullptr);

    Spark::onRenderDebugWorld.addHandler(modId_, +[](void*, Bus::Cursor next) {
        HaloCE::Mod::Mario::debugRender();
        next();
    }, nullptr);

    HaloCE::Mod::Mario::init(modId_);
}

void MarioMod::free() {
    HaloCE::Mod::Mario::free();
}
