#pragma once
#include <vector>
#include <memory>
#include "mod/IMod.hpp"

struct ID3D11DeviceContext;

class ModRegistry {
public:
    // Add a mod before initAll(). Assigns its modId_.
    void add(std::unique_ptr<IMod> mod);

    // Two-phase startup: registerHandlers() all → installAllHooks(base) → init() all.
    void initAll(uintptr_t base);

    // Teardown: free() all (reverse order) → unregisterHandlers per mod → uninstallAllHooks().
    void freeAll();

    // Hot-load after initAll(): assign id, registerHandlers(), init().
    void load(std::unique_ptr<IMod> mod);

    // Hot-unload: free(), unregisterHandlers(id), destroy.
    void unload(IMod* mod);

private:
    std::vector<std::unique_ptr<IMod>> mods_;
    ModId nextId_ = 1;
    bool initialized_ = false;
    uintptr_t base_ = 0;

    ModId assignId(IMod& mod);
};
