#pragma once

#include <string>
#include <sstream>
#include "FourCC.hpp"

namespace Strings {
    std::string fourccToString(uint32_t fourcc);
    uint32_t stringToFourcc(const std::string & str);

    template <typename T>
    inline std::string toHex(T value) {
        std::stringstream ss;
        ss << std::uppercase << std::hex << value;
        return ss.str();
    }

    std::string convertWideString(const std::wstring & wideStr);
}
