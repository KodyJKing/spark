#pragma once

#include <vector>
#include <functional>
#include <cstddef>
#include <cstdint>
#include "MinHook.h"
#include "utils/UnloadLock.hpp"

template<uintptr_t Offset, typename Ret, typename... Args>
struct Hook {
    using Fn      = Ret(*)(Args...);
    using Next    = std::function<Ret(Args...)>;
    using Handler = std::function<Ret(Next, Args...)>;

    inline static Fn       original = nullptr;
    inline static uintptr_t base    = 0;
    inline static std::vector<Handler> handlers;

    static void addHandler(Handler h) { handlers.push_back(std::move(h)); }

    static void install(uintptr_t dllBase) {
        base = dllBase;
        void* target = (void*)(base + Offset);
        MH_CreateHook(target, (void*)&detour, (void**)&original);
        MH_EnableHook(target);
    }

    static void uninstall() {
        if (!base) return;
        MH_RemoveHook((void*)(base + Offset));
        handlers.clear();
        original = nullptr;
        base = 0;
    }

private:
    static Ret detour(Args... args) {
        UnloadLock lock;
        return dispatch(0, args...);
    }

    static Ret dispatch(size_t index, Args... args) {
        if (index >= handlers.size())
            return original(args...);
        Next next = [index](Args... a) { return dispatch(index + 1, a...); };
        return handlers[index](next, args...);
    }
};
