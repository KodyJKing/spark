#include "DynamicGeometry.hpp"

#include "engine/halo1.hpp"
#include "./Coordinates.hpp"
#include "BSPConversion.hpp"
#include "ModelSwap.hpp"
#include "MarioBSPChunk.hpp"
#include "Mario.hpp"
#include "functions/MoveElevatorSurfaceObject.hpp"

#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <mutex>

#include "decomp/surface_terrains.h"

#define DEBUG_DYNAMIC_GEOMETRY 1

#ifdef DEBUG_DYNAMIC_GEOMETRY
    #include "spark/overlay/ESP.hpp"
    #include <iostream>
    #define ISO_TIME_STRING std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
    #define LOG_TIME "[" << ISO_TIME_STRING << "] "
    #define LOG(x) std::cout << "[DynamicGeometry] " << LOG_TIME << x << std::endl;
#else
    #define LOG(x) ;
#endif
namespace Mod::Mario::DynamicGeometry {

    static std::mutex s_updateMutex;

    bool isElevatorTag(Engine::Tag* tag) {
        if (!tag) return false;
        const char* path = tag->getResourcePath();
        if (!path) return false;
        return strstr(path, "elevator") != nullptr 
            || strstr(path, "platform") != nullptr
            || strstr(path, "lift") != nullptr;
    }

    bool isElevatorEntity(Engine::Entity* entity) {
        auto tag = entity->tag();
        if (!tag) return false;
        return isElevatorTag(tag);
    }

    float getAllocateRange(Engine::Tag* entityTag) {
        if (isElevatorTag(entityTag)) return 100.0f;
        return 8.0f;
    }

    float getDeallocateRange(Engine::Tag* entityTag) {
        if (isElevatorTag(entityTag)) return 200.0f;
        return 10.0f;
    }

    struct BoneEntry {
        uint32_t surfaceObjectId;
        std::vector<SM64Surface> surfaces;
    };

    struct EntityBoneKey {
        Engine::Entity* entity;
        size_t boneIndex;
        bool operator==(const EntityBoneKey& other) const {
            return entity == other.entity && boneIndex == other.boneIndex;
        }
    };

    struct EntityBoneKeyHash {
        size_t operator()(const EntityBoneKey& key) const {
            size_t h = std::hash<Engine::Entity*>{}(key.entity);
            h ^= std::hash<size_t>{}(key.boneIndex) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    std::unordered_map<EntityBoneKey, BoneEntry, EntityBoneKeyHash> objectMap;
    std::unordered_set<Engine::Entity*> entitiesWithGeometry;

    std::vector<SM64Surface> convertCollisionBSPToSM64Surfaces(Engine::Entity* entity, size_t boneIndex) {
        if (ModelSwap::hasModelSwapFor(entity, boneIndex)) {
            std::vector<SM64Surface> surfaces;
            ModelSwap::getModelSwapSurfacesFor(entity, boneIndex, surfaces);
            return surfaces;
        }

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

            // auto bspSurfaces = Mod::Mario::BSPConversion::convertBSP(bsp, SURFACE_NOT_SLIPPERY);
            auto bspSurfaces = Mod::Mario::BSPConversion::convertBSP(bsp, SURFACE_HANGABLE);
            surfaces.insert(surfaces.end(), bspSurfaces.begin(), bspSurfaces.end());
        }
        
        return surfaces;
    }

    void getTransform(Engine::Entity* entity, size_t boneIndex, SM64ObjectTransform& transform) {
        if (entity->worldBones.count() == 0) return;
        Engine::Transform* bone = entity->worldBones.get(entity, boneIndex);

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
        objectMap[{entity, boneIndex}] = std::move(entry);
    }

    void allocateDynamicGeometryForEntity(Engine::Entity* entity) {
        if (entitiesWithGeometry.contains(entity)) return;
        entitiesWithGeometry.insert(entity);
        
        auto entityTag = entity->tag();
        if (entityTag) {
            LOG("Creating dynamic geometry for entity: " << entity << " [" << entityTag->getResourcePath() << "]");
            LOG("Entity has " << entity->worldBones.count() << " bones");
        }

        auto boneCount = entity->worldBones.count();
        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            allocateDynamicGeometryForBone(entity, boneIndex);
        }
    }


    void deallocateDynamicGeometryForEntity(Engine::Entity* entity) {
        if (!entitiesWithGeometry.contains(entity)) return;
        entitiesWithGeometry.erase(entity);
        
        auto entityTag = entity->tag();
        LOG("Deallocating dynamic geometry for entity: " << entity << " [" << (entityTag ? entityTag->getResourcePath() : "null") << "]");

        std::erase_if(objectMap, [entity](const auto& kv) {
            if (kv.first.entity == entity) {
                sm64_surface_object_delete(kv.second.surfaceObjectId);
                return true;
            }
            return false;
        });
    }

    void updateObjectTransform(Engine::Entity* entity, SM64MarioState& marioState) {
        if (!entitiesWithGeometry.contains(entity)) return;

        bool isElevator = isElevatorEntity(entity);
        auto boneCount = entity->worldBones.count();

        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            auto it = objectMap.find({entity, boneIndex});
            if (it == objectMap.end()) continue;

            SM64ObjectTransform transform;
            getTransform(entity, boneIndex, transform);

            if (isElevator) {
                moveElevatorSurfaceObject(it->second.surfaceObjectId, transform, marioState);
            } else {
                sm64_surface_object_move(it->second.surfaceObjectId, &transform);
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

    Vec3 getEntityPosition(Engine::Entity* entity) {
        if (entity->worldBones.count() == 0) return entity->pos;
        Engine::Transform* bone = entity->worldBones.get(entity, 0);
        return bone ? bone->pos : entity->pos;
    }

    void cleanStale() {
        std::erase_if(objectMap, [](const auto& kv) {
            if (kv.first.entity->tagID == NULL_HANDLE) {
                sm64_surface_object_delete(kv.second.surfaceObjectId);
                return true;
            }
            return false;
        });
        std::erase_if(entitiesWithGeometry, [](Engine::Entity* entity) {
            return entity->tagID == NULL_HANDLE;
        });
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

            auto tag = entity->tag();
            float allocateRange = getAllocateRange(tag);
            float deallocateRange = getDeallocateRange(tag);
            
            Vec3 entityPos = getEntityPosition(entity);
            float distance = (entityPos - marioWorldPos).lengthSquared();
            if (distance < allocateRange * allocateRange) {
                bool isRegistered = std::any_of(objectMap.begin(), objectMap.end(),
                    [entity](const auto& kv) { return kv.first.entity == entity; });
                if (isRegistered) {
                    updateObjectTransform(entity, marioState);
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
        LOG("Clearing all dynamic geometry objects (" << objectMap.size() << " entries)");
        for (auto& [key, entry] : objectMap) {
            LOG(" - Deleting surface object ID: " << entry.surfaceObjectId << " with " << entry.surfaces.size() << " surfaces");
            sm64_surface_object_delete(entry.surfaceObjectId);
        }
        entitiesWithGeometry.clear();
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

        // for (auto& [key, entry] : objectMap) {
        //     for (auto& surface : entry.surfaces) {
        //         Vec3 v0 = { (float) surface.vertices[0][0], (float) surface.vertices[0][1], (float) surface.vertices[0][2] };
        //         Vec3 v1 = { (float) surface.vertices[1][0], (float) surface.vertices[1][1], (float) surface.vertices[1][2] };
        //         Vec3 v2 = { (float) surface.vertices[2][0], (float) surface.vertices[2][1], (float) surface.vertices[2][2] };

        //         Vec3 haloV0 = Coordinates::marioLocalToHaloWorld(v0, marioChunk);
        //         Vec3 haloV1 = Coordinates::marioLocalToHaloWorld(v1, marioChunk);
        //         Vec3 haloV2 = Coordinates::marioLocalToHaloWorld(v2, marioChunk);

        //         ESP::drawLine(haloV0, haloV1, 0x1F00FFFF);
        //         ESP::drawLine(haloV1, haloV2, 0x1F00FFFF);
        //         ESP::drawLine(haloV2, haloV0, 0x1F00FFFF);
        //     }
        // }

        // Render a window with stats about the dynamic geometry system
        ImGui::Begin("Dynamic Geometry Debug");
        ImGui::Text("Active Surface Objects: %d", objectMap.size());
        size_t totalSurfaces = 0;
        for (const auto& [key, entry] : objectMap) {
            totalSurfaces += entry.surfaces.size();
        }
        ImGui::Text("Total Surfaces: %d", totalSurfaces);

        if (ImGui::CollapsingHeader("Entities")) {
            if (ImGui::BeginTable("EntitiesTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Entity");
                ImGui::TableSetupColumn("Bone");
                ImGui::TableSetupColumn("Surface Object ID");
                ImGui::TableSetupColumn("Surface Count");
                ImGui::TableHeadersRow();
                
                for (const auto& [key, entry] : objectMap) {
                    auto entityTag = key.entity->tag();
                    const char* entityName = entityTag ? entityTag->getResourcePath() : "null";
                    
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", entityName);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", (int)key.boneIndex);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", entry.surfaceObjectId);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%zu", entry.surfaces.size());
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
        #endif
    }

}
