#pragma once

#include "TagBrowserHost.hpp"

namespace Mod::DevTools {
    // In-game host: browses the running game's tags, or a map file from disk if one is opened.
    TagBrowserHost& inGameTagBrowserHost();
}
