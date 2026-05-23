#pragma once
#include <vector>
#include <memory>
#include "spark/mod/IMod.hpp"

struct ID3D11DeviceContext;

namespace Spark {

struct IModDeleter {
    void operator()(IMod* p) const { p->destroy(); }
};

class ModRegistry {
public:
    // Add a mod before initAll(). Assigns its modId_.
    void add(IMod* mod);

    // Two-phase startup: registerHandlers() all → installAllHooks(base) → init() all.
    void initAll(uintptr_t base);

    // Teardown: free() all (reverse order) → unregisterHandlers per mod → uninstallAllHooks().
    void freeAll();

    // Hot-load after initAll(): assign id, registerHandlers(), init().
    void load(IMod* mod);

    // Hot-unload: free(), unregisterHandlers(id), destroy.
    void unload(IMod* mod);

private:
    std::vector<std::unique_ptr<IMod, IModDeleter>> mods_;
    ModId nextId_ = 1;
    bool initialized_ = false;
    uintptr_t base_ = 0;

    ModId assignId(IMod& mod);
};

} // namespace Spark
