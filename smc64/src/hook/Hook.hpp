#pragma once

#include <cstdint>
#include "MinHook.h"
#include "utils/UnloadLock.hpp"
#include "hook/EventBus.hpp"

template<uintptr_t Offset, typename Ret, typename... Args>
struct Hook {
    using Bus     = EventBus<Ret, Args...>;
    using Fn      = Ret(*)(Args...);
    using Handler = typename Bus::Handler;
    using Next    = typename Bus::Next;

    inline static Bus       bus;
    inline static Fn        original = nullptr;
    inline static uintptr_t base     = 0;

    static void addHandler(ModId owner, Handler h, int priority = 0) {
        bus.addHandler(owner, std::move(h), priority);
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
        MH_RemoveHook((void*)(base + Offset));
        bus.clear();
        original = nullptr;
        base = 0;
    }

private:
    static Ret detour(Args... args) {
        UnloadLock ulock;
        return bus.dispatch(original, args...);
    }
};
