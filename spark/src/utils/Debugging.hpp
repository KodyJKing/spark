#pragma once

#include <windows.h>
#include <cstdio>
#include <cstdlib>

#include <thread>

namespace Debugging {

    inline void debug(DWORD timeout = 10000) {
        #ifdef _DEBUG
        if (IsDebuggerPresent()) {
            __debugbreak();
            return;
        }

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "vsjitdebugger.exe -p %lu", GetCurrentProcessId());
        system(cmd);

        DWORD deadline = GetTickCount() + timeout;
        while (!IsDebuggerPresent() && GetTickCount() < deadline)
            Sleep(100);
            
        if (IsDebuggerPresent())
            __debugbreak();
        #endif
    }

    inline bool debugAssert(bool condition, const char* message = "Assertion failed", bool warn = true) {
        #ifdef _DEBUG
        if (!condition) {
            printf("%s\n", message);
            debug();
            if (!warn)
                std::abort();
        }
        #endif
        return !condition;
    }

}
