#include <stdint.h>

namespace Engine::Decomp {

    struct VertexBufferSlice {
        int16_t strideIndex;
        char unk0[2];
        uint32_t vertexCount;
        uint32_t vertexOffset;
        char unk1[4];
        uint32_t bufferSourceIndex;
    };
    static_assert(sizeof(VertexBufferSlice) == 0x14, "VertexBufferSlice size is not 0x14");
}
