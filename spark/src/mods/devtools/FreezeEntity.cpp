#include "FreezeEntity.hpp"
#include "imgui.h"
#include "spark/hook/Hooks.hpp"
#include <unordered_map>

namespace Mod::DevTools::FreezeEntity {

    std::unordered_map<Engine::Entity*, bool> frozenEntities;
    std::unordered_map<Engine::EntityCategory, bool> frozenCategories;
    std::unordered_map<uint32_t, bool> frozenTagHandles;

    void init(Spark::ModId modId) {
        Spark::UpdateEntity::addHandler(modId, +[](void*, auto next, uint32_t entityHandle) -> uint64_t {
            auto entity = Engine::getEntityPointer(entityHandle);
            if (entity && frozenEntities[entity]) return 0;
            if (entity && frozenCategories[(Engine::EntityCategory)entity->entityCategory]) return 0;
            if (entity && frozenTagHandles[entity->tagID]) return 0;
            return next(entityHandle);
        }, nullptr);
    }

    bool& entityFrozen(Engine::Entity* entity) {
        if (!frozenEntities.contains(entity))
            frozenEntities[entity] = false;
        return frozenEntities[entity];
    }

    bool& categoryFrozen(Engine::EntityCategory category) {
        if (!frozenCategories.contains(category))
            frozenCategories[category] = false;
        return frozenCategories[category];
    }

    bool& tagHandleFrozen(uint32_t tagHandle) {
        if (!frozenTagHandles.contains(tagHandle))
            frozenTagHandles[tagHandle] = false;
        return frozenTagHandles[tagHandle];
    }

} // namespace Mod::DevTools
