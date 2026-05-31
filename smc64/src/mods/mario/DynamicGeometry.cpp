#include "DynamicGeometry.hpp"

#include "engine/halo1.hpp"
#include "./Coordinates.hpp"
#include "BSPConversion.hpp"
#include "MarioBSPChunk.hpp"
#include "Mario.hpp"

#include <unordered_map>
#include <filesystem>
#include <iostream>

#include "decomp/surface_terrains.h"
namespace HaloCE::Mod::Mario::DynamicGeometry {

    float allocateRange = 10.0f;
    float deallocateRange = 20.0f;

    struct ObjectEntry {
        uint32_t surfaceObjectId;
        std::vector<SM64Surface> surfaces;
    };

    std::unordered_map<Engine::Entity*, std::vector<ObjectEntry>> objectMap;

    std::vector<SM64Surface> convertCollisionBSPToSM64Surfaces(Engine::Entity* entity, size_t boneIndex) {
        auto entityTag = entity->tag();
        if (entityTag == nullptr) return {};
        auto collisionTag = getCollisionGeometryTag(entityTag);
        if (collisionTag == nullptr) return {};
        auto collisionData = (Engine::CollisionTagData*) collisionTag->getData();
        if (collisionData == nullptr) return {};

        auto nodeCount = collisionData->collisionNodes.count;
        if (nodeCount <= boneIndex) return {};
        auto node = collisionData->collisionNodes.get<Engine::CollisionNode>(boneIndex);
        if (node == nullptr) return {};

        std::vector<SM64Surface> surfaces;
        
        auto bspCount = node->collisionBsps.count;
        for (size_t bspIndex = 0; bspIndex < bspCount; ++bspIndex) {
            auto bsp = node->collisionBsps.get<Engine::CollisionBSP>(bspIndex);
            if (bsp == nullptr) continue;

            auto bspSurfaces = HaloCE::Mod::BSPConversion::convertBSP(bsp, SURFACE_NOT_SLIPPERY);
            surfaces.insert(surfaces.end(), bspSurfaces.begin(), bspSurfaces.end());
        }
        
        return surfaces;
    }

    void getTransform(Engine::Entity* entity, size_t boneIndex, SM64ObjectTransform& transform) {
        if (entity->worldBones.count() == 0) return;
        Engine::WorldTransform* bone = entity->worldBones.get(entity, boneIndex);

        Vec3 marioSpacePos = Coordinates::haloToMario(bone->pos);
        Vec3 chunkOrigin = Coordinates::marioChunkOrigin(MarioBSPChunk::getLoadedChunk());
        Vec3 localPos = marioSpacePos - chunkOrigin;
        transform.position[0] = localPos.x;
        transform.position[1] = localPos.y;
        transform.position[2] = localPos.z;

        Vec3 eulerRotation = orientationToEulerAngles(bone->x, bone->z) * (180.0f / 3.14159265f);
        transform.eulerRotation[0] = eulerRotation.y;
        transform.eulerRotation[1] = eulerRotation.x;
        transform.eulerRotation[2] = eulerRotation.z;
    }

    void allocateDynamicGeometryForEntity(Engine::Entity* entity) {
        // // Get the tag name
        // auto entityTag = entity->tag();
        // if (entityTag) {
        //     std::cout << "Creating dynamic geometry for entity: " << entityTag->getResourcePath() << std::endl;
        //     auto boneCount = entity->worldBones.count();
        //     std::cout << "Bone count: " << boneCount << std::endl;
        // }

        objectMap[entity] = {};
        
        auto boneCount = entity->worldBones.count();
        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            Engine::WorldTransform* bone = entity->worldBones.get(entity, boneIndex);
            
            SM64SurfaceObject surfaceObject = {};
            auto surfaces = convertCollisionBSPToSM64Surfaces(entity, boneIndex);
            if (surfaces.empty()) return;
            surfaceObject.surfaceCount = static_cast<uint32_t>(surfaces.size());
            surfaceObject.surfaces = surfaces.data();
    
            getTransform(entity, boneIndex, surfaceObject.transform);
    
            uint32_t surfaceObjectId = sm64_surface_object_create(&surfaceObject);
            
            ObjectEntry entry;
            entry.surfaceObjectId = surfaceObjectId;
            entry.surfaces = std::move(surfaces);
            objectMap[entity].push_back(std::move(entry));
        }
    }

    void deallocateDynamicGeometryForEntity(Engine::Entity* entity) {
        // Placeholder for deallocation logic
        auto it = objectMap.find(entity);
        if (it != objectMap.end()) {
            for (auto& entry : it->second) {
                sm64_surface_object_delete(entry.surfaceObjectId);
            }
            objectMap.erase(it);
        }
    }

    void updateObjectTransform(Engine::Entity* entity) {
        auto boneCount = entity->worldBones.count();
        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            SM64ObjectTransform transform;
            getTransform(entity, boneIndex, transform);
            if (boneIndex < objectMap[entity].size()) {
                sm64_surface_object_move(objectMap[entity][boneIndex].surfaceObjectId, &transform);
            }
        }
    }

    bool shouldCreateGeometryFor(Engine::Entity* entity) {
        if (!entity) return false;
        auto category = entity->entityCategory;
        if (category == Engine::EntityCategory_Scenery) return true;
        if (category == Engine::EntityCategory_Vehicle) return true;
        if (category == Engine::EntityCategory_Machine) return true;
        return false;
    }
    
    void update(SM64MarioState& marioState) {

        Vec3 marioWorldPos = Mario::marioWorldPosition();

        Engine::foreachEntityRecord([&](Engine::EntityRecord* entityRecord) {
            if (!entityRecord) return;
            auto entity = entityRecord->entity();
            if (!entity) return;

            if (!shouldCreateGeometryFor(entity)) return;
            
            Vec3 entityPos = entity->pos;
            float distance = (entityPos - marioWorldPos).lengthSquared();
            if (distance < allocateRange * allocateRange) {
                if (objectMap.find(entity) != objectMap.end()) {
                    updateObjectTransform(entity);
                } else {
                    allocateDynamicGeometryForEntity(entity);
                }
            } else if (distance > deallocateRange * deallocateRange) {
                deallocateDynamicGeometryForEntity(entity);
            }
        });
    }

    void clearAll() {
        for (auto& kv : objectMap) {
            for (auto& entry : kv.second) {
                sm64_surface_object_delete(entry.surfaceObjectId);
            }
        }
        objectMap.clear();
    }

    void onLoadedChunkChanged(Vec3i /*oldChunk*/, Vec3i /*newChunk*/) {
        clearAll();
    }
    
    void free() {
        clearAll();
    }

}
