#pragma once

#include "Hook.hpp"
#include "engine/halo1.hpp"
#include "spark/RenderBuses.hpp"

namespace Spark {

// Generate a named type alias for each hook.
#define HOOK(Name, Ret, Offset, ...) using Name = Hook<Offset, Ret, __VA_ARGS__>;
#include "HookTable.hpp"
#undef HOOK


    inline void installAllHooks(uintptr_t base) {
        #define HOOK(Name, ...) Name::install(base);
        #include "HookTable.hpp"
        #undef HOOK
    }

    inline void uninstallAllHooks() {
        #define HOOK(Name, ...) Name::uninstall();
        #include "HookTable.hpp"
        #undef HOOK
    }

    inline void unregisterHookHandlers(ModId owner) {
        #define HOOK(Name, ...) Name::unregisterHandlers(owner);
        #include "HookTable.hpp"
        #undef HOOK
    }

}
