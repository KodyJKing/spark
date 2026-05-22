#pragma once

#include "Hook.hpp"
#include "engine/halo1.hpp"

// Generate a named type alias for each hook.
#define HOOK(Name, Ret, Offset, ...) using Name = Hook<Offset, Ret, __VA_ARGS__>;
#include "hooks.def"
#undef HOOK

namespace Spark {

    inline void installAllHooks(uintptr_t base) {
        #define HOOK(Name, ...) Name::install(base);
        #include "hooks.def"
        #undef HOOK
    }

    inline void uninstallAllHooks() {
        #define HOOK(Name, ...) Name::uninstall();
        #include "hooks.def"
        #undef HOOK
    }

    inline void unregisterAllHooksForMod(ModId owner) {
        #define HOOK(Name, ...) Name::unregisterAllHooksForMod(owner);
        #include "hooks.def"
        #undef HOOK
    }

}
