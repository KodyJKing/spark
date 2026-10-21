#pragma once

#include <cstddef>

namespace Engine::Map {

    // Must match [a-zA-Z0-9_ \.\\-]+, be at least 3 characters long and contain at least one backslash.
    inline bool isValidTagPath(const char* path) {
        if (!path) return false;
        int backslashCount = 0;
        for (size_t i = 0; i < 512; i++) {
            char c = path[i];
            if (c == 0)
                return backslashCount > 0 && i > 2;
            if (
                !(c >= 'a' && c <= 'z') &&
                !(c >= 'A' && c <= 'Z') &&
                !(c >= '0' && c <= '9') &&
                c != '_' && c != ' ' &&
                c != '\\' && c != '.' && c != '-'
            )
                return false;
            if (c == '\\')
                backslashCount++;
        }
        return true;
    }

}
