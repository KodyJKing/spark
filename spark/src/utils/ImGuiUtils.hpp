#pragma once
#include <cstdarg>

namespace ImGuiUtils {
    void renderCopyableText(const char* label, const char* text);

    // printf style version of renderCopyableText
    void renderCopyableTextf(const char* label, const char* fmt, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        renderCopyableText(label, buffer);
    }
}
