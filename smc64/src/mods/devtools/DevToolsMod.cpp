#include "DevToolsMod.hpp"
#include "mods/devtools/ESPState.hpp"
#include "mods/devtools/ESPWindow.hpp"
#include "mods/devtools/ESPWorld.hpp"
#include "mods/devtools/DevWindow.hpp"
#include "spark/RenderBuses.hpp"
#include "hook/Hooks.hpp"
#include "haloce/mod/Mod.hpp"
#include "engine/halo1.hpp"

void DevToolsMod::init() {
    using Bus = EventBus<void>;

    UpdateEntity::addHandler(modId_, +[](void*, UpdateEntity::Cursor next, uint32_t entityHandle) -> uint64_t {
        auto rec = Engine::getEntityRecord(entityHandle);
        if (!rec) return next(entityHandle);
        auto entity = rec->entity();
        if (!entity) return next(entityHandle);
        if (HaloCE::Mod::settings.freezeTime) {
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

