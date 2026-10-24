#pragma once

#include "ui/tag_browser/TagBrowserHost.hpp"

namespace SparkLib::UI {
    void showTagBrowserWindow();
    bool isTagBrowserWindowOpen();
    void tagBrowser(TagBrowserHost& host);
}
