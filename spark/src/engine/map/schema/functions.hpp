#pragma once

#include "schema.hpp"
#include <array>
#include <string>
#include <random>

namespace Engine::Map {
    
    inline char randomHexChar() {
        static const char hexChars[] = "0123456789abcdef";
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        return hexChars[dis(gen)];
    }

    inline Id createId() {
        char id[16];
        for (int i = 0; i < 16; ++i) {
            id[i] = randomHexChar();
        }
        return std::string(id, 16);
    }

}
