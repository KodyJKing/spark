#include "SparkCoreMod.hpp"
#include "Engine/halo1.hpp"
#include "Engine/map/spark/spark_texture_buffer.hpp"
#include "spark/hook/Hooks.hpp"

static bool loadingSparkOwnedBitmap = false;

void SparkCoreMod::init() {

    Spark::TextureCacheStartLoadingBitmap::addHandler(modId_, +[](void*, auto next, Engine::Map::BitmapData* bitmapData) {
        loadingSparkOwnedBitmap = Engine::Map::Spark::isSparkOwnedBitmap(bitmapData->bitmapTagHandle);
        uint64_t result = next(bitmapData);
        loadingSparkOwnedBitmap = false;
        return result;
    }, nullptr);

    Spark::CacheReadFile::addHandler(modId_, +[](void*, auto next, uint64_t param_1, long dataOffset, uint32_t dataSize, void* buffer, bool** finishedOut, uint32_t bytesRead, int cacheFileId) {
        // The engine shits the bed if this doesn't run, so we're going to be wasteful for now and run it unconditionally.
        auto result = next(param_1, dataOffset, dataSize, buffer, finishedOut, bytesRead, cacheFileId);
        
        if (loadingSparkOwnedBitmap) {
            char* sparkBuffer = (char*) Engine::Map::Spark::getSparkTextureBuffer();
            memcpy(buffer, sparkBuffer + dataOffset, dataSize);
        }

        return result;
    }, nullptr);

}
