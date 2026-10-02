#pragma once
#include <cstdint>

namespace Engine::Map::Spark {

    void markSparkOwnedBitmap(uint32_t bitmapTagHandle);
    
    bool isSparkOwnedBitmap(uint32_t bitmapTagHandle);

    uint32_t allocateTextureData(size_t size, void** outData);

    void* getSparkTextureBuffer();

    void resetTextureBuffer();

}
