#include <stdint.h>

namespace Engine::Decomp {
    enum Topology : uint16_t {
        Topology_TriangleList,
        Topology_TriangleStrip,
        Topology_Undefined,
        Topology_PointList,
        Topology_LineStrip
    };

    struct IndexBufferSlice {
        Topology topology;
        char unk0, unk1;
        uint32_t indexCount;
        uint32_t indexOffset;
        uint32_t bufferSourceIndex;
    };
    static_assert(sizeof(IndexBufferSlice) == 0x10, "IndexBufferSlice size is not 0x10");
}
