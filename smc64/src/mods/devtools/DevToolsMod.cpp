#include "DevToolsMod.hpp"
#include "mods/devtools/ESPState.hpp"
#include "mods/devtools/ESPWindow.hpp"
#include "mods/devtools/ESPWorld.hpp"
#include "mods/devtools/DevWindow.hpp"
#include "spark/RenderBuses.hpp"

void DevToolsMod::init() {
    using Bus = EventBus<void>;

    Spark::onRenderDebugUI.addHandler(modId_, +[](void*, Bus::Cursor next) {
        Mod::DevTools::checkHotKeys();
        if (Mod::DevTools::showEsp) Mod::DevTools::renderESP();
        next();
    }, nullptr);

    Spark::onRenderPauseMenuTabs.addHandler(modId_, +[](void*, Bus::Cursor next) {
        Mod::DevTools::renderPauseMenuTabs();
        next();
    }, nullptr);

    Spark::onRenderDebugWorld.addHandler(modId_, +[](void*, Bus::Cursor next) {
        if (Mod::DevTools::showEsp) Mod::DevTools::renderDebugWorld();
        next();
    }, nullptr);
}

