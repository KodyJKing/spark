#include "spark_texture_buffer.hpp"
#include <cstdint>
#include <unordered_set>

namespace Engine::Map::Spark {
    
    std::unordered_set<uint32_t> sparkOwnedTags;
    std::vector<char> buffer;

    void markSparkOwnedBitmap(uint32_t bitmapTagHandle) {
        sparkOwnedTags.insert(bitmapTagHandle);
    }

    bool isSparkOwnedBitmap(uint32_t bitmapTagHandle) {
        return sparkOwnedTags.contains(bitmapTagHandle);
    }

    uint32_t allocateTextureData(size_t size, void** outData) {
        uint32_t offset = buffer.size();
        buffer.resize(offset + size);
        if (outData) {
            *outData = buffer.data() + offset;
        }
        return offset;
    }

    void *getSparkTextureBuffer() {
        return buffer.data();
    }

    void resetTextureBuffer() {
        sparkOwnedTags.clear();
        buffer.clear();
    }

}
