#pragma once

#include "ui/tag_browser/TagBrowserHost.hpp"

namespace Mod::DevTools {
    // In-game host: browses the running game's tags, or a map file from disk if one is opened.
    SparkLib::UI::TagBrowserHost& inGameTagBrowserHost();
}
