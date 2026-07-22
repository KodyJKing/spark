#pragma once

#include "../common.hpp"
#include "../tag.hpp"
#include <vector>
#include <type_traits>
#include "math/Vectors.hpp"

#include "../math.hpp"

#include "types.hpp"
#include "entity_functions.hpp"
#include "spark/SparkAPI.h"

namespace Engine {
    SPARK_API bool areEntitiesLoaded();
    SPARK_API Entity* getEntityPointer( EntityRecord* pRecord );
    SPARK_API Entity* getEntityPointer( uint32_t entityHandle );
    SPARK_API uint32_t entityHandleFromIndex(uint16_t index);
    SPARK_API EntityRecord *getEntityRecord(uint32_t entityHandle);
    SPARK_API EntityRecord* getEntityRecord( EntityList* pEntityList, uint32_t entityHandle );

    // Raw C-ABI-safe callback types (function pointer + context). Safe to call across a
    // DLL boundary, unlike std::function (implementation-defined layout/allocator).
    using EntityRecordCallback = void(*)( void* ctx, EntityRecord* rec, uint16_t index );
    using EntityCallback = void(*)( void* ctx, Entity* entity );

    SPARK_API void foreachEntityRecordIndexed( EntityRecordCallback cb, void* ctx );
    SPARK_API void foreachEntityInRadius( const Vec3& center, float radius, EntityCallback cb, void* ctx );

    // Header-only convenience wrappers — instantiated per call site, so ordinary capturing
    // lambdas keep working. Only the trampoline (a plain function pointer) crosses into
    // Engine's actual implementation, never the lambda/closure itself.
    template<typename Fn>
    inline void foreachEntityRecordIndexed( Fn&& cb ) {
        foreachEntityRecordIndexed(
            +[]( void* ctx, EntityRecord* rec, uint16_t index ) { (*static_cast<std::remove_reference_t<Fn>*>(ctx))( rec, index ); },
            &cb
        );
    }

    template<typename Fn>
    inline void foreachEntityRecord( Fn&& cb ) {
        foreachEntityRecordIndexed( [&cb]( EntityRecord* rec, uint16_t ) { cb( rec ); } );
    }

    template<typename Fn>
    inline void foreachEntityInRadius( const Vec3& center, float radius, Fn&& cb ) {
        foreachEntityInRadius(
            center, radius,
            +[]( void* ctx, Entity* entity ) { (*static_cast<std::remove_reference_t<Fn>*>(ctx))( entity ); },
            &cb
        );
    }
}
