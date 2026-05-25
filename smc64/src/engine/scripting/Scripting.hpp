#pragma once

namespace Engine::Scripting {

    // Submit a raw console/HaloScript expression for evaluation.
    // Handles normalization, compilation, and execution in one call.
    // Accepts bare variable names ("rasterizer_wireframe") or full HaloScript
    // expressions ("(set rasterizer_wireframe true)").
    void submit(const char* input);

}
