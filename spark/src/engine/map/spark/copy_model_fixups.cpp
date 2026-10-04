#include "../model.hpp"
#include "../map_file.hpp"
#include "spark_buffers.hpp"

namespace Engine::Map::Spark {

    // Assumes toPart was naively copied, with its data offset still pointing into fromMap.
    bool copyPartVertexFixup(
        MapFile* fromMap,
        MapFile* toMap,
        Part* toPart
    ) {
        if (!fromMap || !toMap || !toPart) 
            return false;
        
        void* fromBuffer = fromMap->fromRelative(toPart->union0.vertices);
        if (!fromBuffer) return false;

        void* toBuffer = nullptr;
        size_t dataSize = toPart->vertexDataSize();
        uint32_t toBufferOffset = allocateVertexData(dataSize, &toBuffer);

        memcpy(toBuffer, fromBuffer, dataSize);

        toPart->runtimeVertices = { toBufferOffset };
        toPart->union0.bufferSourceIndex = 1;

        markSparkOwnedPart(toPart);

        return true;
    }

    // Assumes toPart was naively copied, with its data offset still pointing into fromMap.
    bool copyPartIndexFixup(
        MapFile* fromMap,
        MapFile* toMap,
        Part* toPart
    ) {
        if (!fromMap || !toMap || !toPart) 
            return false;
        
        void* fromBuffer = fromMap->fromRelative(toPart->indices);
        if (!fromBuffer) return false;

        void* toBuffer = nullptr;
        size_t dataSize = toPart->indexDataSize();
        uint32_t toBufferOffset = allocateIndexData(dataSize, &toBuffer);

        memcpy(toBuffer, fromBuffer, dataSize);

        toPart->indices = { toBufferOffset };

        return true;
    }

    // Assumes toModel was naively copied, with its data offset still pointing into fromMap.
    bool copyModelFixup(
        MapFile* fromMap,
        MapFile* toMap,
        Model* toModel,
        uint32_t toModelTagHandle
    ) {
        if (!fromMap || !toMap || !toModel)
            return false;

        bool allPassed = true;
        for (size_t i = 0; i < toModel->geometries.count; i++) {
            Geometry* toGeometry = toMap->getBlockElement(toModel->geometries, i);
            if (!toGeometry) {
                allPassed = false;
                continue;
            }
            for (size_t j = 0; j < toGeometry->parts.count; j++) {
                Part* toPart = toMap->getBlockElement(toGeometry->parts, j);
                if (!toPart) {
                    allPassed = false;
                    continue;
                }
                allPassed &= copyPartVertexFixup(fromMap, toMap, toPart);
                allPassed &= copyPartIndexFixup(fromMap, toMap, toPart);
            }
        }

        markSparkOwnedTag(toModelTagHandle);
    }

}
