#include "engine/common.hpp"
#include "engine/rendering/index.hpp"
#include "types/IndexBufferSlice.hpp"
#include "types/VertexBufferSlice.hpp"
#include "types/BufferSource.hpp"
#include <d3d11.h>

using namespace Engine::Decomp;

#define MAX_PRIMITIVES_PER_DRAW 10000

namespace {

    // Ghidra: PTR_bufferSources (offset 0x2E5F770) - table of BufferSource*, indexed directly
    // by bufferSourceIndex; indices >= 0x8000 are treated as "no source" (null).
    constexpr uintptr_t kBufferSourceTableOffset = 0x2E5F770;
    // Ghidra: SHORT_strides (offset 0x18B8210) - vertex stride in bytes, indexed by strideIndex.
    constexpr uintptr_t kStrideTableOffset = 0x18B8210;
    // Ghidra: D3D11_PRIMITIVE_TOPOLOGIES (offset 0x18A1D18) - Topology -> D3D11_PRIMITIVE_TOPOLOGY.
    constexpr uintptr_t kPrimitiveTopologyTableOffset = 0x18A1D18;

    BufferSource* bufferSourceAt(uint32_t sourceIndex) {
        if (sourceIndex >= 0x8000) return nullptr;
        auto* table = reinterpret_cast<BufferSource**>(Engine::dllBase() + kBufferSourceTableOffset);
        return table[sourceIndex];
    }

    int16_t strideFor(int16_t strideIndex) {
        auto* table = reinterpret_cast<int16_t*>(Engine::dllBase() + kStrideTableOffset);
        return table[strideIndex];
    }

    D3D11_PRIMITIVE_TOPOLOGY d3dTopologyFor(Topology topology) {
        auto* table = reinterpret_cast<D3D11_PRIMITIVE_TOPOLOGY*>(Engine::dllBase() + kPrimitiveTopologyTableOffset);
        return table[topology];
    }

    // Not yet reversed - called as raw function pointers into the real binary for now.
    // Ghidra: looks like a cached IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0)
    // wrapper (0x39 == DXGI_FORMAT_R16_UINT) that skips the call if already bound.
    using SetIndexBufferCached_t = void(*)(ID3D11Buffer* indexBuffer);
    constexpr uintptr_t kSetIndexBufferCachedOffset = 0xB15C0;
    // Ghidra: flushes pending blend/depth-stencil/rasterizer/sampler state changes.
    using FlushPendingRenderState_t = void(*)();
    constexpr uintptr_t kFlushPendingRenderStateOffset = 0xB3DB28;
    // Ghidra: purpose unidentified.
    using UnknownRenderCall_t = void(*)();
    constexpr uintptr_t kUnknownRenderCall1Offset = 0xBF619C;
    constexpr uintptr_t kUnknownRenderCall2Offset = 0xBF61C8;

    SetIndexBufferCached_t setIndexBufferCached() {
        return (SetIndexBufferCached_t)(Engine::dllBase() + kSetIndexBufferCachedOffset);
    }
    FlushPendingRenderState_t flushPendingRenderState() {
        return (FlushPendingRenderState_t)(Engine::dllBase() + kFlushPendingRenderStateOffset);
    }
    UnknownRenderCall_t unknownRenderCall1() {
        return (UnknownRenderCall_t)(Engine::dllBase() + kUnknownRenderCall1Offset);
    }
    UnknownRenderCall_t unknownRenderCall2() {
        return (UnknownRenderCall_t)(Engine::dllBase() + kUnknownRenderCall2Offset);
    }

}

// Ghidra: _drawModel. Draws up to `remainingPrimitivesToDraw` primitives from one index/vertex
// buffer pair, batching at most 10000 primitives per DrawIndexed call. `param_2` is a dead
// argument in this function (only ever spilled to its shadow-stack slot, never read).
void _drawModel(IndexBufferSlice *indices, uint64_t param_2, int remainingPrimitivesToDraw,
                VertexBufferSlice *vertices) {
    if (remainingPrimitivesToDraw <= 0) return;

    ID3D11DeviceContext* context = Engine::getD3D11Context();
    int indicesOffset = 0;

    do {
        if (indices == nullptr) return;
        if (indices->bufferSourceIndex == 0) return;
        if (vertices == nullptr) return;

        uint32_t sourceIndex = vertices->bufferSourceIndex;
        if (sourceIndex == 0) return;

        int numPrimitivesThisIter = remainingPrimitivesToDraw > MAX_PRIMITIVES_PER_DRAW ? MAX_PRIMITIVES_PER_DRAW : remainingPrimitivesToDraw;

        UINT stride = (UINT)(int)strideFor(vertices->strideIndex);
        UINT vertexOffset = vertices->vertexOffset;
        ID3D11Buffer* vertexBuffer = bufferSourceAt(sourceIndex)->object->getBuffer();
        ID3D11Buffer* indexBuffer = bufferSourceAt(indices->bufferSourceIndex)->object->getBuffer();

        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &vertexOffset);
        setIndexBufferCached()(indexBuffer);
        context->IASetPrimitiveTopology(d3dTopologyFor(indices->topology));
        flushPendingRenderState()();
        unknownRenderCall1()();
        unknownRenderCall2()();

        // TriangleList draws N*3 indices; every other topology (strip/point/line) draws N+2.
        int numIndices = indices->topology == Topology_TriangleList
            ? numPrimitivesThisIter * 3
            : numPrimitivesThisIter + 2;

        context->DrawIndexed(numIndices, ((int)indices->indexOffset >> 1) + indicesOffset, 0);

        remainingPrimitivesToDraw -= numPrimitivesThisIter;
        if (indices->topology == Topology_TriangleList) {
            indicesOffset += numPrimitivesThisIter * 3;
        } else if (indices->topology == Topology_TriangleStrip) {
            indicesOffset += numPrimitivesThisIter;
        }
    } while (remainingPrimitivesToDraw > 0);
}
