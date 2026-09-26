#pragma once

#include "tag_data_types.hpp"
#include "utils/Strings.hpp"

namespace Engine::Map {
    std::string Tag::groupIdString() {
        auto fourccA = Strings::fourccToString( groupID );
        auto fourccB = Strings::fourccToString( parentGroupID );
        auto fourccC = Strings::fourccToString( grandparentGroupID );
        return "[" + fourccC + " > " + fourccB + " > " + fourccA + "]";
    }
}
