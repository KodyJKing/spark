#pragma once

#include <string>

#define FOUR_CC(a, b, c, d) ((a << 24) | (b << 16) | (c << 8) | d)
#define FOUR_CC_STR(str) FOUR_CC(str[0], str[1], str[2], str[3])

namespace Strings {
    std::string fourccToString(uint32_t fourcc);
    uint32_t stringToFourcc(const std::string & str);
}
