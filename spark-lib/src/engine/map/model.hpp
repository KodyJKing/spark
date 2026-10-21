#pragma once
#include "tag_data_types.hpp"
#include "math/Vectors.hpp"

namespace Engine::Map {

    // Vertex sizes by vertex format.
    static inline uint16_t vertexSizes[20] = { 0x38, 0x20, 0x14, 0x08, 0x44, 0x20, 0x18, 0x24, 0x18, 0x10, 0x14, 0x14, 0x20, 0x08, 0x20, 0x20, 0x24, 0x1C, 0x00, 0x00 };

    struct Instance {
        char pad0[0x20];
    };

    struct Marker {
        char pad0[0x34];
        BlockPointerTyped<Instance> instances;
    };
    
    struct Node {
        char pad0[0x9C];
    };

    struct Permutation {
        char pad0[0x85];
    };

    struct Region {
        char pad0[0x40];
        BlockPointerTyped<Permutation> permutations;
    };
    
    struct Part {
        uint16_t shaderIndex;
        char pad0[0x12];
        Vec3 centroid;
        char pad1[0x24];
        uint16_t topology;
        char pad2[0x02];
        uint32_t indexCount;
        Pointer<PointerBase_Indices> indices;
        char pad3[0x04];
        uint32_t vertexFormat; // Todo: Enum
        uint32_t vertexCount;
        Pointer<PointerBase_Vertices> runtimeVertices;
        char pad4[0x04];
        union {
            Pointer<PointerBase_Vertices> vertices;
            uint32_t bufferSourceIndex;
        } union0;
        char pad5[0x1C];

        inline size_t vertexDataSize() { return vertexSizes[vertexFormat] * vertexCount; }
        inline size_t indexDataSize() { return sizeof(uint16_t) * indexCount; }
    };
    static_assert(offsetof(Part, centroid) == 0x14);
    static_assert(offsetof(Part, topology) == 0x44);
    static_assert(offsetof(Part, union0.vertices) == 0x64);
    static_assert(offsetof(Part, union0.bufferSourceIndex) == 0x64);
    static_assert(sizeof(Part) == 0x84);

    struct Geometry {
        char pad0[0x24];
        BlockPointerTyped<Part> parts;
    };

    struct Shader {
        TagReference shader;
        uint16_t permutation;
        char pad0[0x0E];
    };
    static_assert(sizeof(Shader) == 0x20);

    struct Model {
        char pad0[0xAC];
        BlockPointerTyped<Marker> markers;
        BlockPointerTyped<Node> nodes;
        BlockPointerTyped<Region> regions;
        BlockPointerTyped<Geometry> geometries;
        BlockPointerTyped<Shader> shaders;
    };
    static_assert(offsetof(Model, shaders) == 0xDC);

}
