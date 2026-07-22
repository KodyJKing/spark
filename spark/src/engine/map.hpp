#pragma once

#include <stdint.h>
#include "spark/SparkAPI.h"

namespace Engine {

    #pragma pack(push, 1)
    struct MapHeader {
        uint32_t magicHeader; // Should be 1751474532 ('head' in ascii fourcc)
        uint32_t cacheVersion;
        uint32_t fileSize;
        uint32_t paddingLength; // Only used on Xbox
        uint32_t tagDataOffset;
        uint32_t tagDataSize;
        char pad0[8];
        char mapName[32];
        char buildVersion[32];
        uint32_t scenarioType;
        uint32_t checksum;
        char pad1[0x794];
        uint32_t magicFooter; // Should be 1718579060 ('foot' in ascii fourcc)
    };
    #pragma pack(pop)
    
    SPARK_API char* getMapName();
    SPARK_API MapHeader* getMapHeader();
    SPARK_API bool isOnMap( const char* mapName );
    SPARK_API bool isMapLoaded();
    SPARK_API uint64_t translateMapAddress( uint32_t address );
    SPARK_API uint32_t translateToMapAddress(uint64_t absoluteAddress);

}