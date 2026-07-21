#pragma once

#include <vector>
#include <algorithm>
#include <cstddef>
#include <shared_mutex>
#include "spark/mod/ModId.hpp"

// Pure middleware dispatch — no MinHook coupling.
// Handlers are raw function pointers + void* context (C-ABI-safe, no heap per dispatch).
// Captureless lambdas convert implicitly; use + to force the conversion at the call site.
// Lower priority value = outermost (runs first). Default priority = 0.
namespace Spark {

template<typename Ret, typename... Args>
struct EventBus {
    struct Cursor;
    using TerminalFn = Ret(*)(void* ctx, Args...);
    using AdvanceFn  = Ret(*)(Cursor self, Args...);
    using HandlerFn  = Ret(*)(void* ctx, Cursor next, Args...);

    // Lightweight value type passed to each handler as its `next` continuation.
    // Trivially copyable; no heap allocation. `advance` is set at dispatch time
    // and always refers to the owning DLL's instantiation.
    struct Cursor {
        AdvanceFn  advance;
        void*      busData;
        size_t     index;
        TerminalFn termFn;
        void*      termCtx;

        Ret operator()(Args... args) const { return advance(*this, args...); }
    };

    struct Entry {
        ModId     owner;
        int       priority;
        HandlerFn fn;
        void*     ctx;
    };

    std::vector<Entry> handlers;
    std::shared_mutex  handlersMutex;
    TerminalFn defaultHandler;

    void addHandler(ModId owner, HandlerFn fn, void* ctx, int priority = 0) {
        std::unique_lock lock(handlersMutex);
        auto it = std::lower_bound(handlers.begin(), handlers.end(), priority,
            [](const Entry& e, int p) { return e.priority < p; });
        handlers.insert(it, { owner, priority, fn, ctx });
    }
    void addHandler(ModId owner, TerminalFn fn, int priority = 0) {
        auto handler = +[](void* ctx, Cursor next, Args... args) {
            next(args...);
            return static_cast<TerminalFn>(ctx)(nullptr, args...);
        };
        addHandler(owner, handler, fn, priority);
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

    Ret dispatch(TerminalFn termFn, void* termCtx, Args... args) {
        // Todo: Think through lock's implications for hooked recursive functions.
        // If a unique_lock is sandwiched between two shared locks in the same thread, what happens?
        std::shared_lock slock(handlersMutex);
        Cursor cursor { &EventBus::doAdvance, this, 0, termFn, termCtx };
        return doAdvance(cursor, args...);
    }
    Ret dispatch(Args... args) {
        return dispatch(defaultHandler, nullptr, args...);
    }

    EventBus() : defaultHandler(nullptr) {}
    EventBus(TerminalFn defaultHandler) : defaultHandler(defaultHandler) {}

private:
    static Ret doAdvance(Cursor cursor, Args... args) {
        auto* self = static_cast<EventBus*>(cursor.busData);
        if (cursor.index >= self->handlers.size())
            return cursor.termFn(cursor.termCtx, args...);
        const auto& e = self->handlers[cursor.index];
        Cursor next = cursor;
        next.index++;
        return e.fn(e.ctx, next, args...);
    }
};

} // namespace Spark
