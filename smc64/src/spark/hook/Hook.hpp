#pragma once

#include <cstdint>
#include "MinHook.h"
#include "utils/UnloadLock.hpp"
#include "spark/EventBus.hpp"

namespace Spark {

template<uintptr_t Offset, typename Ret, typename... Args>
struct Hook {
    using Bus     = EventBus<Ret, Args...>;
    using Fn      = Ret(*)(Args...);
    using Cursor  = typename Bus::Cursor;
    using Handler = typename Bus::HandlerFn;

    inline static Bus       bus;
    inline static Fn        original = nullptr;
    inline static uintptr_t base     = 0;

    static void addHandler(ModId owner, Handler fn, void* ctx, int priority = 0) {
        bus.addHandler(owner, fn, ctx, priority);
    }

    static void unregisterHandlers(ModId owner) {
        bus.unregisterHandlers(owner);
    }

    static void install(uintptr_t dllBase) {
        base = dllBase;
        void* target = (void*)(base + Offset);
        MH_CreateHook(target, (void*)&detour, (void**)&original);
        MH_EnableHook(target);
    }

    static void uninstall() {
        if (!base) return;
        MH_DisableHook((void*)(base + Offset));
        MH_RemoveHook((void*)(base + Offset));
        bus.clear();
        original = nullptr;
        base = 0;
    }

private:
    // Passes &original (an object pointer) as ctx — avoids fn-ptr → void* cast.
    static Ret terminalShim(void* ctx, Args... args) {
        return (*static_cast<Fn*>(ctx))(args...);
    }

    static Ret detour(Args... args) {
        UnloadLock ulock;
        return bus.dispatch(terminalShim, &original, args...);
    }
};

} // namespace Spark
