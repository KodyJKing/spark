#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include <cstddef>
#include <shared_mutex>
#include "mod/ModId.hpp"

// Pure middleware dispatch — no MinHook coupling.
// Handlers form an ordered chain; each receives a `next` continuation.
// Lower priority value = outermost (runs first). Default priority = 0.
template<typename Ret, typename... Args>
struct EventBus {
    using Next     = std::function<Ret(Args...)>;
    using Handler  = std::function<Ret(Next, Args...)>;
    using Terminal = std::function<Ret(Args...)>;

    struct Entry {
        ModId   owner;
        int     priority;
        Handler handler;
    };

    std::vector<Entry> handlers;
    std::shared_mutex  handlersMutex;

    void addHandler(ModId owner, Handler h, int priority = 0) {
        std::unique_lock lock(handlersMutex);
        auto it = std::lower_bound(handlers.begin(), handlers.end(), priority,
            [](const Entry& e, int p) { return e.priority < p; });
        handlers.insert(it, { owner, priority, std::move(h) });
    }

    void unregisterHandlers(ModId owner) {
        std::unique_lock lock(handlersMutex);
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                [owner](const Entry& e) { return e.owner == owner; }),
            handlers.end());
    }

    void clear() {
        std::unique_lock lock(handlersMutex);
        handlers.clear();
    }

    // Dispatch through the handler chain; `terminal` is called when the chain is exhausted.
    Ret dispatch(Terminal terminal, Args... args) {
        std::shared_lock slock(handlersMutex);
        return dispatchImpl(0, terminal, args...);
    }

private:
    Ret dispatchImpl(size_t index, const Terminal& terminal, Args... args) {
        if (index >= handlers.size())
            return terminal(args...);
        Next next = [this, index, &terminal](Args... a) {
            return dispatchImpl(index + 1, terminal, a...);
        };
        return handlers[index].handler(next, args...);
    }
};
