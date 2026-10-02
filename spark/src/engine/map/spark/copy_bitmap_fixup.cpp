#include "../bitmap.hpp"
#include "../map_file.hpp"
#include "spark_texture_buffer.hpp"

// #define DEBUG
#ifdef DEBUG
#include <iostream>
#include <Windows.h>
#include "utils/Debugging.hpp"
#define LOG(X) std::cout << "[copy_bitmap_fixup] " << X << std::endl
#define BEEP(T) Beep(1000, T)
#define DEBUGGER Debugging::debug();
#else
#define LOG(X)
#define BEEP(T)
#define DEBUGGER
#endif

namespace Engine::Map::Spark {

    // Assumes toBitmap was naively copied, with its data offset still pointing into fromMap.
    bool copyBitmapDataFixup(
        MapFile* fromMap,
        MapFile* toMap,
        SparkBitmapData* toBitmap,
        uint32_t toBitmapTagHandle
    ) {
        if (!fromMap || !toBitmap || !toMap) 
            return false;

        void* fromBufferData = fromMap->fromRelative(toBitmap->data);
        if (!fromBufferData) return false;
        
        
        void* toBufferData = nullptr;
        uint32_t toBufferOffset = allocateTextureData(toBitmap->dataSize, &toBufferData);

        memcpy(toBufferData, fromBufferData, toBitmap->dataSize);

        // This would be correct if we stored texture data in Halo's texture buffer:
        //   toBitmap->data = toMap->toRelative<PointerBase_Map>(toBufferData);
        // Instead we just use the spark buffer relative offset.
        // This offset is (mostly) harmless nonsense without Spark's texture buffer binding detour.
        toBitmap->data = { toBufferOffset };

        markSparkOwnedBitmap(toBitmapTagHandle);
        toBitmap->bitmapTagHandle = toBitmapTagHandle;

        return true;
    }

    // Assumes toBitmap to have undergone naive-copy + block-pointer-fixup.
    bool copyBitmapFixup(
        MapFile* fromMap,
        MapFile* toMap,
        SparkBitmap* toBitmap,
        uint32_t toBitmapTagHandle
    ) {
        if (!fromMap || !toBitmap || !toMap)
            return false;

        bool allPassed = true;
        for (size_t i = 0; i < toBitmap->bitmapData.count; i++) {
            SparkBitmapData* toBitmapData = toMap->getBlockElement(toBitmap->bitmapData, i);
            if (!toBitmapData) {
                allPassed = false;
                continue;
            }
            allPassed &= copyBitmapDataFixup(fromMap, toMap, toBitmapData, toBitmapTagHandle);
        }
        
        return allPassed;
    }

};
