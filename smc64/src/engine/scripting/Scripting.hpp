#pragma once
#include <cstdint>

namespace Engine::Scripting {

    // Submit a raw console/HaloScript expression for evaluation.
    // Handles normalization, compilation, and execution in one call.
    // Accepts bare variable names ("rasterizer_wireframe") or full HaloScript
    // expressions ("(set rasterizer_wireframe true)").
    void submit(const char* input);

    struct GlobalSearchResult {
        int16_t index;
        uint16_t flags0;
        uint16_t flags1;
        uint16_t flags2;
    };

    GlobalSearchResult lookupGlobal(const char* name);

    uint32_t readGlobal(int16_t index);
    uint32_t readGlobal(const char* name);
}
