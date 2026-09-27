#pragma once

#include "imgui.h"

namespace Mod::DevTools::DissectTagNew {
    constexpr size_t kMaxDisplayableSize = 4096 << 2;
    constexpr size_t kBytesPerRow = 32;
    constexpr size_t kStripeColor = IM_COL32(100, 100, 100, 255);
}
