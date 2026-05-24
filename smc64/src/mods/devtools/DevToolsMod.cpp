#include "DevToolsMod.hpp"
#include "mods/devtools/ESPState.hpp"
#include "mods/devtools/ESPWindow.hpp"
#include "mods/devtools/ESPWorld.hpp"
#include "mods/devtools/DevWindow.hpp"
#include "spark/RenderBuses.hpp"
#include "spark/hook/Hooks.hpp"
#include "mods/devtools/VectorProfiler.hpp"
#include "engine/halo1.hpp"

void DevToolsMod::init() {
    using Bus = Spark::EventBus<void>;

    Spark::UpdateAllEntities::addHandler(modId_, +[](void*, auto next) {
        Mod::DevTools::VectorProfiler::start(GetCurrentThreadId());
        next();
    }, nullptr);

    Spark::UpdateEntity::addHandler(modId_, +[](void*, auto next, uint32_t entityHandle) -> uint64_t {
        auto rec = Engine::getEntityRecord(entityHandle);
        if (!rec) return next(entityHandle);
        auto entity = rec->entity();
        if (!entity) return next(entityHandle);
        if (Mod::DevTools::freezeTime) {
            auto playerRec = Engine::getPlayerRecord();
            if (playerRec && rec->id != playerRec->id) return 0;
        }
        return next(entityHandle);
    }, nullptr);

    Spark::onRenderDebugUI.addHandler(modId_, +[](void*, Bus::Cursor next) {
        Mod::DevTools::checkHotKeys();
        Mod::DevTools::renderESP();
        next();
    }, nullptr);

    Spark::onRenderPauseMenuTabs.addHandler(modId_, +[](void*, Bus::Cursor next) {
        Mod::DevTools::renderPauseMenuTabs();
        next();
    }, nullptr);

    Spark::onRenderDebugWorld.addHandler(modId_, +[](void*, Bus::Cursor next) {
        Mod::DevTools::renderDebugWorld();
        next();
    }, nullptr);

    Spark::DamageEntity::addHandler(modId_, +[](void*, auto next, Engine::DamageEvent* event, uint32_t entityHandle, uint16_t param_3, uint16_t param_4, int16_t hitBoneIndex, uint64_t param_6) {
        if (Mod::DevTools::invincibility) {
            if (entityHandle == Engine::getPlayerHandle()) 
                return;
        }
        next(event, entityHandle, param_3, param_4, hitBoneIndex, param_6);
    }, nullptr);
}

void DevToolsMod::free() {
    Mod::DevTools::VectorProfiler::stop();
}
