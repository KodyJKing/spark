#include "ModRegistry.hpp"
#include "hook/Hooks.hpp"
#include <algorithm>

ModId ModRegistry::assignId(IMod& mod) {
    mod.modId_ = nextId_++;
    return mod.modId_;
}

void ModRegistry::add(std::unique_ptr<IMod> mod) {
    assignId(*mod);
    mods_.push_back(std::move(mod));
}

void ModRegistry::initAll(uintptr_t base) {
    base_ = base;
    initialized_ = true;
    Spark::installAllHooks(base_);
    for (auto& mod : mods_)
        mod->init();
}

void ModRegistry::freeAll() {
    for (int i = (int)mods_.size() - 1; i >= 0; --i)
        mods_[i]->free();
    for (auto& mod : mods_)
        Spark::unregisterHookHandlers(mod->modId_);
    Spark::uninstallAllHooks();
    mods_.clear();
    initialized_ = false;
}

void ModRegistry::load(std::unique_ptr<IMod> mod) {
    assignId(*mod);
    mod->init();
    mods_.push_back(std::move(mod));
}

void ModRegistry::unload(IMod* target) {
    auto it = std::find_if(mods_.begin(), mods_.end(),
        [target](const std::unique_ptr<IMod>& m) { return m.get() == target; });
    if (it == mods_.end()) return;
    (*it)->free();
    Spark::unregisterHookHandlers((*it)->modId_);

    // Unregister render phase handlers
    Spark::onRenderDebugUI.unregisterHandlers((*it)->modId_);
    Spark::onRenderDebugWorld.unregisterHandlers((*it)->modId_);
    Spark::onRenderPauseMenuTabs.unregisterHandlers((*it)->modId_);

    mods_.erase(it);
}
