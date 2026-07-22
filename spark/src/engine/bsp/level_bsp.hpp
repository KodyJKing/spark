#pragma once
#include <stdint.h>
#include "bsp.hpp"
#include "spark/SparkAPI.h"

namespace Engine {
    SPARK_API uintptr_t getBSPPointer();
    
    SPARK_API uint32_t getBSPVertexCount();
    SPARK_API BSPVertex* getBSPVertexArray();

    SPARK_API uint32_t getBSPEdgeCount();
    SPARK_API BSPEdge* getBSPEdgeArray();

    SPARK_API uint32_t getBSPPlaneCount();
    SPARK_API BSPPlane* getBSPPlaneArray();

    SPARK_API BSPSurface* getBSPSurfaceArray();
    SPARK_API uint32_t getBSPSurfaceCount();
    
    SPARK_API uint64_t getBSPSignature();
}
