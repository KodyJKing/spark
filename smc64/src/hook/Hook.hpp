#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include "MinHook.h"
#include "utils/UnloadLock.hpp"

using ModId = uint32_t;

template<uintptr_t Offset, typename Ret, typename... Args>
struct Hook {
    using Fn      = Ret(*)(Args...);
    using Next    = std::function<Ret(Args...)>;
    using Handler = std::function<Ret(Next, Args...)>;

    struct Entry {
        ModId   owner;
        int     priority;
        Handler handler;
    };

    inline static Fn                          original = nullptr;
    inline static uintptr_t                   base     = 0;
    inline static std::vector<Entry>          handlers;
    inline static std::shared_mutex           handlersMutex;

    // Lower priority value runs first (outermost in the chain).
    static void addHandler(ModId owner, Handler h, int priority = 0) {
        std::unique_lock lock(handlersMutex);
        auto it = std::lower_bound(handlers.begin(), handlers.end(), priority,
            [](const Entry& e, int p) { return e.priority < p; });
        handlers.insert(it, { owner, priority, std::move(h) });
    }

    static void unregisterAll(ModId owner) {
        std::unique_lock lock(handlersMutex);
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                [owner](const Entry& e) { return e.owner == owner; }),
            handlers.end());
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
        {
            std::unique_lock lock(handlersMutex);
            handlers.clear();
        }
        original = nullptr;
        base = 0;
    }

private:
    static Ret detour(Args... args) {
        UnloadLock ulock;
        std::shared_lock slock(handlersMutex);
        return dispatch(0, args...);
    }

    static Ret dispatch(size_t index, Args... args) {
        if (index >= handlers.size())
            return original(args...);
        Next next = [index](Args... a) { return dispatch(index + 1, a...); };
        return handlers[index].handler(next, args...);
    }
};
