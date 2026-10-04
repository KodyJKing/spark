#include "SparkCoreMod.hpp"
#include "ModelBufferDetour.hpp"
#include "engine/halo1.hpp"
#include "engine/map/model.hpp"
#include "engine/map/spark/spark_buffers.hpp"
#include "spark/hook/Hooks.hpp"
#include <stack>

static bool loadingSparkOwnedBitmap = false;
static std::stack<uint32_t> renderingEntityStack;

#define DEBUG
#include <iostream>
#ifdef DEBUG
#define LOG(x) std::cout << "[SparkCoreMod] " << x << std::endl;
#else
#define LOG(x)
#endif

void SparkCoreMod::init() {

    Spark::TextureCacheStartLoadingBitmap::addHandler(modId_, +[](void*, auto next, Engine::Map::BitmapData* bitmapData) {
        loadingSparkOwnedBitmap = Engine::Map::Spark::isSparkOwnedTag(bitmapData->bitmapTagHandle);
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

    Spark::DrawModelPart::addHandler(modId_, +[](void*, auto next, Engine::IndexBufferSlice* indices, uint64_t param_2, int remainingPrimitivesToDraw, Engine::VertexBufferSlice* vertices) -> void {
        char* partBase = (char*) indices - 0x44;
        bool renderingSparkOwnedModel = Engine::Map::Spark::isSparkOwnedPart((Engine::Map::Part*) partBase);
        // bool renderingSparkOwnedModel = true;
        Mod::SparkCore::drawModelPart(indices, param_2, remainingPrimitivesToDraw, vertices, renderingSparkOwnedModel);
    }, nullptr);

}
