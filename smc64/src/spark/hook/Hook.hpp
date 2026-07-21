#pragma once

#include <cstdint>
#include "MinHook.h"
#include "utils/UnloadLock.hpp"
#include "spark/EventBus.hpp"
#include "spark/SparkAPI.h"

namespace Spark {

// Storage (bus/original/base) must have exactly one instance shared by Spark and every
// mod DLL that registers handlers on it — explicitly instantiated + exported once per
// hook in HookInstantiations.cpp. Hooks.hpp declares `extern template` for each hook so
// no other translation unit implicitly instantiates (and thus duplicates) this storage.
// install()/uninstall() touch MinHook directly and are Spark-internal only — mod DLLs
// must only ever call addHandler()/unregisterHandlers().
//
// C4251 (member needs dll-interface) is a false positive here: EventBus<>'s member
// functions just operate in-place on whichever Hook<>::bus storage they're called
// through, and that storage is guaranteed to be a single shared instance via the
// explicit-instantiation + extern-template scheme above, so EventBus<> itself never
// needs its own dllexport/dllimport annotation.
#pragma warning(push)
#pragma warning(disable: 4251)
template<uintptr_t Offset, typename Ret, typename... Args>
struct SPARK_API Hook {
    using Bus     = EventBus<Ret, Args...>;
    using Fn      = Ret(*)(Args...);
    using Cursor  = typename Bus::Cursor;
    using Handler = typename Bus::HandlerFn;

    static Bus       bus;
    static Fn        original;
    static uintptr_t base;

    static void addHandler(ModId owner, Handler fn, void* ctx, int priority = 0) {
        bus.addHandler(owner, fn, ctx, priority);
    }

    static void unregisterHandlers(ModId owner) {
        bus.unregisterHandlers(owner);
    }

    static void install(uintptr_t dllBase) {
        base = dllBase;
        void* target = (void*)(base + Offset);
        MH_CreateHook(target, (void*)&dispatch, (void**)&original);
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

    static Ret dispatch(Args... args) {
        UnloadLock ulock;
        return bus.dispatch(terminalShim, &original, args...);
    }

private:
    // Passes &original (an object pointer) as ctx — avoids fn-ptr → void* cast.
    static Ret terminalShim(void* ctx, Args... args) {
        return (*static_cast<Fn*>(ctx))(args...);
    }
};

template<uintptr_t Offset, typename Ret, typename... Args>
typename Hook<Offset, Ret, Args...>::Bus Hook<Offset, Ret, Args...>::bus;

template<uintptr_t Offset, typename Ret, typename... Args>
typename Hook<Offset, Ret, Args...>::Fn Hook<Offset, Ret, Args...>::original = nullptr;

template<uintptr_t Offset, typename Ret, typename... Args>
uintptr_t Hook<Offset, Ret, Args...>::base = 0;
#pragma warning(pop)

} // namespace Spark
