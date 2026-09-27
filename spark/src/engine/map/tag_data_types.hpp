#pragma once

#include <cstdint>
#include <string>

namespace Engine::Map {

    enum PointerBase {
        PointerBase_Map,
        PointerBase_Tags,
        PointerBase_Vertices,
        PointerBase_Indices,
    };

    template<PointerBase B>
    struct Pointer {
        uint32_t offset;
    };

    struct BlockPointer {
        uint32_t count;
        Pointer<PointerBase_Tags> data;
        uint32_t bullshit;
    };

    struct Tag {
        public:
        uint32_t groupID; // Group ID's are fourcc's
        uint32_t parentGroupID; 
        uint32_t grandparentGroupID;
        uint32_t tagID; 
        Pointer<PointerBase_Tags> path; 
        Pointer<PointerBase_Tags> data;
        char pad_0018[8];

        std::string groupIdString();
    };

    struct TagDataHeader {
        Pointer<PointerBase_Tags> tagArray;
        uint32_t checksum;
        uint32_t scenarioId;
        uint32_t tagCount;
        uint32_t modelPartCount;
        
        Pointer<PointerBase_Map> modelData;
        uint32_t modelPartCount2;
        uint32_t vertexDataSize;
        uint32_t modelDataSize;
        char magic[4]; // Should be "sgat"
    };

    struct CacheHeader {
        char magic[4]; // Should be "deah"
        uint32_t cacheVersion;
        uint32_t fileSize;
        uint32_t paddingLength;
        Pointer<PointerBase_Map> tagData;
        uint32_t tagDataSize;
        char pad0[8];
        char scenarioName[32];
        char buildVersion[32];
        uint16_t scenarioType;
        char pad1[2];
        uint32_t checksum;
    };

}
