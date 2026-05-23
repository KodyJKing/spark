#include "DevToolsMod.hpp"
#include "mods/devtools/ESPState.hpp"
#include "mods/devtools/ESPWindow.hpp"
#include "mods/devtools/ESPWorld.hpp"
#include "mods/devtools/DevWindow.hpp"
#include "spark/RenderBuses.hpp"
#include "spark/hook/Hooks.hpp"
#include "overlay/VectorProfiler.hpp"
#include "engine/halo1.hpp"

void DevToolsMod::init() {
    using Bus = Spark::EventBus<void>;

    Spark::UpdateAllEntities::addHandler(modId_, +[](void*, Spark::UpdateAllEntities::Cursor next) {
        Overlay::ESP::VectorProfiler::start(GetCurrentThreadId());
        next();
    }, nullptr);

    Spark::UpdateEntity::addHandler(modId_, +[](void*, Spark::UpdateEntity::Cursor next, uint32_t entityHandle) -> uint64_t {
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
}

void DevToolsMod::free() {
    Overlay::ESP::VectorProfiler::stop();
}

