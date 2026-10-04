#pragma once
#include <cstdint>
#include <d3d11.h>
#include "../model.hpp"

namespace Engine::Map::Spark {

    void markSparkOwnedTag(uint32_t tagHandle);
    bool isSparkOwnedTag(uint32_t tagHandle);

    void markSparkOwnedPart(Part* part);
    bool isSparkOwnedPart(Part* part);
    
    uint32_t allocateTextureData(size_t size, void** outData);
    uint32_t allocateVertexData(size_t size, void** outData);
    uint32_t allocateIndexData(size_t size, void** outData);
    
    void* getSparkTextureBuffer();
    ID3D11Buffer* getSparkVertexBuffer();
    ID3D11Buffer* getSparkIndexBuffer();

    void resetSparkBuffers();

}
