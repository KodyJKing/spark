#pragma once

#include <stdint.h>
#include "math/Vectors.hpp"
#include "../common.hpp"

namespace Halo1 {

    enum FlareFlags : uint16_t {
        FlareFlags_SpatialUpdateNeeded = 0x02,
        FlareFlags_InSpatialPartition  = 0x04,
        FlareFlags_LensFlareConfirmed  = 0x08,
    };

    class FlareEntry {
        public:
        int16_t  generation;          // 0x00 — non-zero = slot active
        uint16_t flags;               // 0x02 — FlareFlags bitmask
        uint32_t lightTagId;          // 0x04
        uint32_t lightRenderHandle;   // 0x08 — NULL_HANDLE = not currently queued
        char     pad_000C[8];         // 0x0C
        float    colorR;              // 0x14 — zeroing R/G/B prevents render
        float    colorG;              // 0x18
        float    colorB;              // 0x1C
        char     pad_0020[12];        // 0x20
        uint32_t mountEntityHandle;   // 0x2C — NULL_HANDLE = world-fixed (no entity)
        Vec3     orientA;             // 0x30 — first orientation axis (written by updateFlareTransform)
        Vec3     orientB;             // 0x3C — second orientation axis
        Vec3     worldPos;            // 0x48 — world-space position
        float    brightness;          // 0x54 — computed brightness/alpha for this frame
        int32_t  tickTimestamp;       // 0x58 — -1 = static/permanent; >= 0 = spawn tick for fade-in
        uint16_t markerId;            // 0x5C
        uint16_t markerIdB;           // 0x5E — unknown marker/attachment id; used in phase-2 render loop
        int16_t  nodeIndex;           // 0x60 — bone/node index; -1 = use fallback world-space address
        char     pad_0062[22];        // 0x62 — includes unknown field_0x6C used in transform path
        float    scale;               // 0x78
    };
    static_assert( sizeof( FlareEntry ) == 0x7C );
    static_assert( offsetof( FlareEntry, mountEntityHandle ) == 0x2C );
    static_assert( offsetof( FlareEntry, tickTimestamp ) == 0x58 );
    static_assert( offsetof( FlareEntry, scale ) == 0x78 );

    class FlareManager {
        public:
        char     pad_0000[34];        // 0x00
        uint16_t stride;              // 0x22 — entry stride in bytes (always 0x7C)
        char     pad_0024[10];        // 0x24
        int16_t  capacity;            // 0x2E — max flare slots (observed: 326)
        char     pad_0030[4];         // 0x30
        int32_t  entriesOffset;       // 0x34 — byte offset from struct base to first entry (observed: 0x38)

        FlareEntry* entryAtIndex( uint16_t index ) {
            return (FlareEntry*) ( (uintptr_t) this + entriesOffset + index * stride );
        }
    };
    static_assert( offsetof( FlareManager, stride ) == 0x22 );
    static_assert( offsetof( FlareManager, capacity ) == 0x2E );
    static_assert( offsetof( FlareManager, entriesOffset ) == 0x34 );

}
