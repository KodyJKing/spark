#pragma once

#include <stdint.h>
#include "math/Vectors.hpp"

namespace Engine {

    // Corresponds to damageEntity02's first parameter (damageEntity02 in Ghidra).
    // Field offsets verified from decompile; _unk gaps are unconfirmed.
    struct DamageEvent {
        uint32_t damageTypeTagHandle;  // 0x00 - damage effect tag handle
        uint32_t sourceType;           // 0x04 - e.g. 4 = melee
        uint32_t flags;                // 0x08 - bitfield (0x1 = single entity, 0x4 = instant kill, 0x20 = head, 0x100 = ...)
        uint32_t interactorHandle;     // 0x0c - entity handle; looked up in InteractThings
        uint32_t attackerHandle;       // 0x10 - source entity handle
        uint16_t sourceTypeIndex;      // 0x14 - damage source index (-1 = none, 7 = ?)
        uint8_t  _unk0x16[0x16];       // 0x16
        Vec3     hitDirection;         // 0x2c
        Vec3     impactPosition;       // 0x38
        float    baseDamage;           // 0x44
        float    damageMultiplier;     // 0x48 - scaled by shields/vehicle occupant count
        float    resultHealth;         // 0x4c - output: recipient health after damage
        uint16_t materialType;         // 0x50 - surface/material index written from tag data
    };

    static_assert(offsetof(DamageEvent, flags)            == 0x08);
    static_assert(offsetof(DamageEvent, attackerHandle)   == 0x10);
    static_assert(offsetof(DamageEvent, hitDirection)     == 0x2c);
    static_assert(offsetof(DamageEvent, impactPosition)   == 0x38);
    static_assert(offsetof(DamageEvent, materialType)     == 0x50);

}
