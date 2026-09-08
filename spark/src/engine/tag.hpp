// Reverse engineered Halo 1 structures and access functions go here.

#pragma once

#include <stdint.h>
#include <string>
#include "map.hpp"
#include "memory/Memory.hpp"
#include "utils/Strings.hpp"
#include "spark/SparkAPI.h"

namespace Engine {

    enum TagGroupId {
        #define ENTRY(name, value) GroupId_##name = FOUR_CC_STR(#value)
        ENTRY(Weapon, weap),
        ENTRY(Biped, bipd),
        ENTRY(Character, chr),
        ENTRY(Scenario, scen),
        ENTRY(Vehicle, veh),
        ENTRY(Effect, effe),
        ENTRY(Joint, jpt!),
        ENTRY(Material, matg),
        ENTRY(Physics, phy!),
        ENTRY(Projectile, proj),
        ENTRY(Damage, jpt!),
        ENTRY(Device, devi),
        ENTRY(Equipment, eqip),
        ENTRY(Contrail, cont),
        ENTRY(Particle, part),
        ENTRY(Sound, snd!),
        ENTRY(Animation, antr),
        ENTRY(Actor, actr),
        ENTRY(ActorVariant, actv),
        ENTRY(Bitmap, bitm),
        ENTRY(Shader, shdr),
        ENTRY(Light, ligh),
        ENTRY(BSP, sbsp),
        ENTRY(CollisionModel, coll),
        #undef ENTRY
    };

    // Thanks to Kavawuvi for documentation on the map format and tag structure.
    class SPARK_API Tag {
        public:
        uint32_t groupID; // Group ID's are fourcc's
        uint32_t parentGroupID; 
        uint32_t grandparentGroupID;
        uint32_t tagID; 
        uint32_t resourcePathAddress; 
        uint32_t dataAddress; 
        char pad_0018[8]; 

        char* getResourcePath();
        void* getData();
        std::string groupIDStr();
        std::string classIdStr();

        template<typename T>
        T* getDataAs() {
            return (T*) getData();
        }
    };

    template<typename T>
    T* getTagDataAs(Tag* tag) {
        if (!tag) return nullptr;
        return (T*) tag->getData();
    }

    struct CreateTagOptions {
        uint32_t groupId;
        uint32_t parentGroupId;
        uint32_t grandparentGroupId;
        const char* resourcePath;
    };

    // Represents a block of structs in a tag's data.
    struct BlockPointer {
        uint32_t count;
        uint32_t offset;   // Use translateMapAddress to get the actual pointer
        uint32_t bullshit; // Not sure what this does.

        template <typename T>
        T* get(size_t index, bool safe = true) {
            if (index >= count) return nullptr;
            uint64_t baseAddress = Engine::translateMapAddress(offset);
            if (!baseAddress) return nullptr;
            if (safe && !Memory::isAllocated(baseAddress + index * sizeof(T))) return nullptr;
            return (T*)(baseAddress + index * sizeof(T));
        }
    };
    
    SPARK_API uint32_t getTagArraySize();
    SPARK_API Tag *getTag(uint32_t tagID);
    SPARK_API Tag * findTag(const char * path, const char * fourCC);
    SPARK_API Tag * findTag(const char * path, uint32_t fourCC);
    SPARK_API bool validTagPath(const char * path);
    SPARK_API bool tagExists(Tag * tag);

    SPARK_API Tag* allocateTag(CreateTagOptions options);
    SPARK_API void* allocateTagData(size_t size);
    SPARK_API Tag* cloneTagNaive(Tag* tag, size_t dataSize = 0);
}
