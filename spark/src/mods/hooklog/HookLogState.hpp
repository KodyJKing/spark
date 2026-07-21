#pragma once
#include <chrono>
#include <cstdint>

namespace Mod::HookLog {

    struct Toggles {
#define HOOK(Name, ...) bool Name = false;
#include "spark/hook/HookTable.hpp"
#undef HOOK
    };

    struct LastLogTimes {
#define HOOK(Name, ...) uint64_t Name = 0;
#include "spark/hook/HookTable.hpp"
#undef HOOK
    };

    inline Toggles      toggles;
    inline LastLogTimes lastLogTimes;
    inline int          debounceMilliseconds = 100;

    inline bool shouldLog(uint64_t& lastMs) {
        using namespace std::chrono;
        auto now = static_cast<uint64_t>(
            duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
        if (debounceMilliseconds <= 0 || now - lastMs >= static_cast<uint64_t>(debounceMilliseconds)) {
            lastMs = now;
            return true;
        }
        return false;
    }

} // namespace Mod::HookLog
