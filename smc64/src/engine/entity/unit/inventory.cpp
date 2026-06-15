#include "Inventory.hpp"
#include "engine/entity/entity_list.hpp"

namespace Engine {

    Inventory* getInventory(uint32_t entityHandle) {
        auto entity = getEntityPointer(entityHandle);
        if (!entity) return nullptr;
        return (Inventory*)((uintptr_t)entity + 0x2D0);
    }

    Inventory* getInventoryTypeSafe(uint32_t entityHandle) {
        auto entity = getEntityPointer(entityHandle);
        if (!entity) return nullptr;
        if (entity->entityCategory != EntityCategory_Biped) return nullptr;
        return (Inventory*)((uintptr_t)entity + 0x2D0);
    }
    
}
