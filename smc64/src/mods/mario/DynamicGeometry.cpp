#include "DynamicGeometry.hpp"

#include "engine/halo1.hpp"
#include "./Coordinates.hpp"
#include "BSPConversion.hpp"
#include "MarioBSPChunk.hpp"
#include "Mario.hpp"

#include <unordered_map>
#include <filesystem>
#include <mutex>

#include "decomp/surface_terrains.h"

// #define DEBUG_DYNAMIC_GEOMETRY 1

#ifdef DEBUG_DYNAMIC_GEOMETRY
    #include "spark/overlay/ESP.hpp"
    #include <iostream>
    #define LOG(x) std::cout << "[DynamicGeometry] " << x << std::endl;
#else
    #define LOG(x) ;
#endif
namespace HaloCE::Mod::Mario::DynamicGeometry {

    static std::mutex s_updateMutex;

    float allocateRange = 10.0f;
    float deallocateRange = 20.0f;

    struct BoneEntry {
        uint32_t surfaceObjectId;
        std::vector<SM64Surface> surfaces;
    };

    struct EntityEntry {
        Engine::Entity* entity;
        std::unordered_map<size_t /*boneIndex*/, BoneEntry> boneMap;
    };

    std::unordered_map<Engine::Entity*, EntityEntry> objectMap;

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

    void allocateDynamicGeometryForBone(Engine::Entity* entity, size_t boneIndex) {
        Engine::WorldTransform* bone = entity->worldBones.get(entity, boneIndex);
        
        SM64SurfaceObject surfaceObject = {};
        auto surfaces = convertCollisionBSPToSM64Surfaces(entity, boneIndex);
        if (surfaces.empty()) return;
        surfaceObject.surfaceCount = static_cast<uint32_t>(surfaces.size());
        surfaceObject.surfaces = surfaces.data();

        getTransform(entity, boneIndex, surfaceObject.transform);

        uint32_t surfaceObjectId = sm64_surface_object_create(&surfaceObject);
        
        BoneEntry entry;
        entry.surfaceObjectId = surfaceObjectId;
        entry.surfaces = std::move(surfaces);
        objectMap[entity].boneMap[boneIndex] = std::move(entry);
    }

    void allocateDynamicGeometryForEntity(Engine::Entity* entity) {
        auto entityTag = entity->tag();
        if (entityTag) {
            LOG("Creating dynamic geometry for entity: " << entity << " [" << entityTag->getResourcePath() << "]");
            auto boneCount = entity->worldBones.count();
            LOG("Entity has " << boneCount << " bones");
        }

        objectMap[entity] = {};
        
        auto boneCount = entity->worldBones.count();
        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            allocateDynamicGeometryForBone(entity, boneIndex);
        }
    }

    
    void deallocateDynamicGeometryForEntity(Engine::Entity* entity) {
        auto it = objectMap.find(entity);
        if (it != objectMap.end()) {
            auto entityTag = entity->tag();
            LOG("Deallocating dynamic geometry for entity: " << entity << " [" << (entityTag ? entityTag->getResourcePath() : "null") << "]");

            for (auto& [boneIndex, entry] : it->second.boneMap) {
                sm64_surface_object_delete(entry.surfaceObjectId);
            }
            objectMap.erase(it);
        }
    }

    void updateObjectTransform(Engine::Entity* entity) {
        auto boneCount = entity->worldBones.count();

        auto updateTransformForBone = [&](size_t boneIndex) {
            if (boneIndex >= boneCount) return;
            if (objectMap[entity].boneMap.find(boneIndex) == objectMap[entity].boneMap.end()) return;

            SM64ObjectTransform transform;
            getTransform(entity, boneIndex, transform);
            sm64_surface_object_move(objectMap[entity].boneMap[boneIndex].surfaceObjectId, &transform);
        };

        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            updateTransformForBone(boneIndex);
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

    Vec3 getEntityPosition(Engine::Entity* entity) {
        if (entity->worldBones.count() == 0) return entity->pos;
        Engine::WorldTransform* bone = entity->worldBones.get(entity, 0);
        return bone ? bone->pos : entity->pos;
    }

    void cleanStale() {
        std::vector<Engine::Entity*> toDeallocate;

        for (auto& kv : objectMap) {
            Engine::Entity* entity = kv.first;
            if (entity->tagID == NULL_HANDLE) {
                toDeallocate.push_back(entity);
            }
        }

        for (Engine::Entity* entity : toDeallocate) {
            deallocateDynamicGeometryForEntity(entity);
        }
    }
    
    void update(SM64MarioState& marioState) {
        std::lock_guard<std::mutex> updateLock(s_updateMutex);

        Vec3 marioWorldPos = Mario::marioWorldPosition();

        cleanStale();

        Engine::foreachEntityRecord([&](Engine::EntityRecord* entityRecord) {
            if (!entityRecord) return;
            auto entity = entityRecord->entity();
            if (!entity) return;

            if (!shouldCreateGeometryFor(entity)) return;
            
            Vec3 entityPos = getEntityPosition(entity);
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
        std::lock_guard<std::mutex> updateLock(s_updateMutex);
        
        LOG("Clearing all dynamic geometry objects (" << objectMap.size() << " entities)");
        for (auto& kv : objectMap) {
            auto tag = kv.first->tag();
            if (tag) {
                LOG(" - Entity: " << tag->getResourcePath() << " with " << kv.second.boneMap.size() << " surface objects");
            } else {
                LOG(" - Entity: (unknown) with " << kv.second.boneMap.size() << " surface objects");
            }
            for (auto& [boneIndex, entry] : kv.second.boneMap) {
                LOG("   - Deleting surface object ID: " << entry.surfaceObjectId << " with " << entry.surfaces.size() << " surfaces");
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

    void debugRender() {
        #ifdef DEBUG_DYNAMIC_GEOMETRY
        namespace ESP = Spark::Overlay::ESP;

        std::lock_guard<std::mutex> updateLock(s_updateMutex);

        for (auto& kv : objectMap) {
            for (auto& [boneIndex, entry] : kv.second.boneMap) {
                for (auto& surface : entry.surfaces) {
                    Vec3 v0 = { (float) surface.vertices[0][0], (float) surface.vertices[0][1], (float) surface.vertices[0][2] };
                    Vec3 v1 = { (float) surface.vertices[1][0], (float) surface.vertices[1][1], (float) surface.vertices[1][2] };
                    Vec3 v2 = { (float) surface.vertices[2][0], (float) surface.vertices[2][1], (float) surface.vertices[2][2] };

                    Vec3 haloV0 = Coordinates::marioLocalToHaloWorld(v0, marioChunk);
                    Vec3 haloV1 = Coordinates::marioLocalToHaloWorld(v1, marioChunk);
                    Vec3 haloV2 = Coordinates::marioLocalToHaloWorld(v2, marioChunk);


                    ESP::drawLine(haloV0, haloV1, 0x1F00FFFF);
                    ESP::drawLine(haloV1, haloV2, 0x1F00FFFF);
                    ESP::drawLine(haloV2, haloV0, 0x1F00FFFF);
                }
            }
        }
        #endif
    }

}
