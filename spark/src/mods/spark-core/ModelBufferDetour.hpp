#pragma once
#include "engine/halo1.hpp"

namespace Mod::SparkCore {
    void drawModelPart(Engine::IndexBufferSlice* indices, uint64_t param_2, int remainingPrimitivesToDraw, Engine::VertexBufferSlice* vertices, bool renderingSparkOwnedModel);
}
