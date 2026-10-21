#include "spark_buffers.hpp"
#include "engine/rendering/index.hpp"
#include <cstdint>
#include <unordered_set>

// Todo: Make this dynamic.
#define SPARK_VERTEX_BUFFER_SIZE 0x800000
#define SPARK_INDEX_BUFFER_SIZE 0x80000

#define DEBUG

#ifdef DEBUG
#include <iostream>
#define LOG(x) std::cout << "[Engine::Map::Spark - spark_buffers] " << x << std::endl;
#else
#define LOG(x)
#endif

namespace {

    uint32_t allocateData(std::vector<char>& buffer, size_t size, void** outData) {
        uint32_t offset = buffer.size();
        buffer.resize(offset + size);
        if (outData) *outData = buffer.data() + offset;
        return offset;
    }

    struct D3DBuffer {
        std::vector<char> cpuBuffer;
        ID3D11Buffer* gpuBuffer = nullptr;
        bool dirty = false;

        uint32_t allocateData(size_t size, void** outData) {
            if (size == 0) return 0;
            dirty = true;
            return ::allocateData(cpuBuffer, size, outData);
        }

        void createGPUBuffer(UINT size, UINT bindFlags) {
            auto device = Engine::getD3D11Device();
            if (!device) return;
            D3D11_BUFFER_DESC desc{0};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth = size;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            HRESULT result = device->CreateBuffer(&desc, nullptr, &gpuBuffer);
            #ifdef DEBUG
            if (FAILED(result)) {
                static int tick = 0;
                if (tick++ % 60 == 0) {
                    LOG("Failed to create GPU buffer " << reinterpret_cast<void*>(result));
                }
            }
            #endif
        }

        void updateGPUBuffer() {
            if (!dirty) return;
            auto ctx = Engine::getD3D11Context();
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(ctx->Map(gpuBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                memcpy(mapped.pData, cpuBuffer.data(), cpuBuffer.size());
                ctx->Unmap(gpuBuffer, 0);
            }
            dirty = false;
        }

        ID3D11Buffer* getGPUBuffer(UINT size, UINT bindFlags) {
            if (!gpuBuffer) createGPUBuffer(size, bindFlags);
            updateGPUBuffer();
            return gpuBuffer;
        }

        void release() {
            if (!gpuBuffer) return;
            gpuBuffer->Release();
            gpuBuffer = nullptr;
        }
    };
}

namespace Engine::Map::Spark {
    
    std::unordered_set<uint32_t> sparkOwnedTags;
    std::unordered_set<Part*> sparkOwnedParts;

    std::vector<char> textureBuffer;
    D3DBuffer vertexBuffer;
    D3DBuffer indexBuffer;

    void markSparkOwnedTag(uint32_t tagHandle) { sparkOwnedTags.insert(tagHandle); }
    bool isSparkOwnedTag(uint32_t tagHandle) { return sparkOwnedTags.contains(tagHandle); }

    void markSparkOwnedPart(Part* part) { sparkOwnedParts.insert(part); }
    bool isSparkOwnedPart(Part* part) { return sparkOwnedParts.contains(part); }

    uint32_t allocateTextureData(size_t size, void** outData) { return allocateData(textureBuffer, size, outData); }
    uint32_t allocateVertexData(size_t size, void** outData) {
        #ifdef DEBUG
        auto result = vertexBuffer.allocateData(size, outData);
        LOG("Vertex data allocated at: " << *outData);
        return result;
        #else
        return vertexBuffer.allocateData(size, outData);
        #endif
    }
    uint32_t allocateIndexData(size_t size, void** outData) { return indexBuffer.allocateData(size, outData); }

    void *getSparkTextureBuffer() {
        return textureBuffer.data();
    }

    ID3D11Buffer* getSparkVertexBuffer() {
        return vertexBuffer.getGPUBuffer(SPARK_VERTEX_BUFFER_SIZE, D3D11_BIND_VERTEX_BUFFER);
    }

    ID3D11Buffer* getSparkIndexBuffer() {
        return indexBuffer.getGPUBuffer(SPARK_INDEX_BUFFER_SIZE, D3D11_BIND_INDEX_BUFFER);
    }

    void resetSparkBuffers() {
        sparkOwnedTags.clear();
        textureBuffer.clear();
        sparkOwnedParts.clear();
        vertexBuffer.cpuBuffer.clear();
        indexBuffer.cpuBuffer.clear();
        vertexBuffer.release();
        indexBuffer.release();
    }

}
