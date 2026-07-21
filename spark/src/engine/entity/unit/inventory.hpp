#pragma once
#include <stdint.h>

namespace Engine {
 
    struct Inventory {
        char pad0[3];
        uint8_t activeWeaponIndex;
        char pad1[1];
        uint8_t activeWeaponIndex2; // Is this leftover from dual wielding support? Seems always to mirror activeWeaponIndex.
        uint8_t activeGrenadeIndex;
        uint8_t activeGrenadeIndex2; // ???
        uint32_t weaponHandles[4];
        uint32_t weaponData[4];
        char pad2[4];
        uint8_t fragCount;
        uint8_t plasmaCount;
        char pad3[2]; // Probably counts for more grenade types.

        uint32_t activeWeaponHandle() const {
            if (activeWeaponIndex >= 4) return 0xFFFFFFFF;
            return weaponHandles[activeWeaponIndex];
        }
    };

    Inventory *getInventory(uint32_t entityHandle);

    Inventory *getInventoryTypeSafe(uint32_t entityHandle);
}
 