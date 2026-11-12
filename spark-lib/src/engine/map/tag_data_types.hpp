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

    template<PointerBase B, typename T = void>
    struct Pointer {
        uint32_t offset;
    };

    template<typename T = void>
    struct BlockPointerTyped {
        uint32_t count;
        Pointer<PointerBase_Tags, T> data;
        uint32_t bullshit;
    };

    using BlockPointer = BlockPointerTyped<void>;

    template<typename T = void>
    struct DataPointerTyped {
        int32_t size;
        uint32_t bullshit;
        int fileOffset;
        Pointer<PointerBase_Tags, T> data; 
        void* definition;
    };

    using DataPointer = DataPointerTyped<void>;
    
    struct TagReference {
        uint32_t groupID;
        Pointer<PointerBase_Tags> path; 
        uint32_t bullshit;
        uint32_t tagHandle;
    };

    struct Tag {
        public:
        uint32_t groupID; // Group ID's are fourcc's
        uint32_t parentGroupID; 
        uint32_t grandparentGroupID;
        uint32_t tagHandle; 
        Pointer<PointerBase_Tags> path; 
        Pointer<PointerBase_Tags> data;
        char pad_0018[8];

        std::string groupIdString();
    };

    struct TagDataHeader {
        Pointer<PointerBase_Tags> tagArray;
        uint32_t scenarioId;
        uint32_t checksum;
        uint32_t tagCount;
        uint32_t modelPartCount;
        
        Pointer<PointerBase_Map> modelData;
        uint32_t modelPartCount2;
        uint32_t vertexDataSize;
        uint32_t modelDataSize;
        char magic[4]; // Should be "sgat"
    };
    static_assert(offsetof(TagDataHeader, scenarioId) == 0x04);
    static_assert(offsetof(TagDataHeader, checksum) == 0x08);

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
        uint32_t unused[485];
        char footerMagic[4]; // Should be "toof"
    };

    struct StructureBspHeader {
        Pointer<PointerBase_Tags> structureBsp;
        int vertexBufferSize;
        Pointer<PointerBase_Map> vertexBuffer;
        int pad[2];
        char magic[4]; // Should be "psbs"
    };

    struct StructureBspReference {
        Pointer<PointerBase_Map> structureBspHeader;
        uint32_t size;
        Pointer<PointerBase_Tags> runtimeAddress;
        int pad[1];
        TagReference tagReference;
    };

    struct Scenario {
        char pad[0x5A4];
        BlockPointerTyped<StructureBspReference> structureBsps;
    };

}
