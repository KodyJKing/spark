#pragma once

#include "spark/mod/ModId.hpp"
#include <string>
#include <unordered_map>

namespace HaloCE::Mod::Mario::MarioWeaponOffset {

    // Position offset expressed in the left-hand bone's local coordinate frame.
    //   x = along hand X (typically forward / grip axis)
    //   y = along hand Y
    //   z = along hand Z
    struct Offset {
        float x = 0.05f;
        float y = 0.0f;
        float z = 0.0f;
    };

    // Per-weapon-tag offsets. Key = tag resource path (e.g. "weapons\\pistol\\pistol").
    extern std::unordered_map<std::string, Offset> offsetMap;

    // Active offset being edited in the debug window.
    extern Offset offset;

    // Registers an onRenderDebugUI handler that draws the weapon-offset tuning window.
    void registerHandlers(Spark::ModId modId);

    void getWeaponOffset(uint32_t weaponHandle, Offset& outOffset);

}
