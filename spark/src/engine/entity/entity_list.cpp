#include "entity_list.hpp"
#include "memory/Memory.hpp"
#include <iostream>

namespace Engine {

    EntityList* getEntityListPointer() { return *(EntityList**) ( dllBase() + 0x1C42248 ); }
    uintptr_t getEntityArrayBase() { return *(uintptr_t*) ( dllBase() + 0x2D9CDF8 ); }

    bool isEntityListLoaded() { return Memory::isAllocated( (uintptr_t) getEntityListPointer() ); }
    bool isEntityArrayLoaded() { return Memory::isAllocated( (uintptr_t) getEntityArrayBase() ); }
    bool areEntitiesLoaded() { return isEntityListLoaded() && isEntityArrayLoaded(); }

    Entity* EntityRecord::entity() { return getEntityPointer( this ); }

    EntityRecord* getEntityRecord( EntityList* pEntityList, uint32_t entityHandle ) {
        if ( entityHandle == 0xFFFFFFFF ) return nullptr;
        uint32_t i = entityHandle & 0xFFFF;
        auto recordAddress = (uintptr_t) pEntityList + pEntityList->entityListOffset + i * sizeof( EntityRecord );
        return (EntityRecord*) recordAddress;
    }
    EntityRecord* getEntityRecord( uint32_t entityHandle ) { return getEntityRecord( getEntityListPointer(), entityHandle ); }

    Entity* getEntityPointer( EntityRecord* pRecord ) {
        if ( !pRecord || pRecord->entityArrayOffset == -1 ) return nullptr;
        uintptr_t entityAddress = getEntityArrayBase() + 0x34 + (INT_PTR) pRecord->entityArrayOffset;
        return (Entity*) entityAddress;
    }

    Entity* getEntityPointer( uint32_t entityHandle ) {
        return getEntityPointer( getEntityRecord( entityHandle ) );
    }

    uint32_t entityHandleFromIndex( uint16_t index ) {
        auto rec = getEntityRecord( index );
        if ( !rec ) return 0xFFFFFFFF;
        return rec->id << 16 | index;
    }

    void foreachEntityRecordIndexed( EntityRecordCallback cb, void* ctx ) {
        auto pEntityList = getEntityListPointer();
        if ( !pEntityList ) return;
        for ( uint16_t i = 0; i < pEntityList->capacity; i++ ) {
            auto pRecord = getEntityRecord( pEntityList, i );
            if ( pRecord->entityArrayOffset == -1 ) continue;
            cb( ctx, pRecord, i );
        }
    }

    // Todo: Optimize this. Use halo engine's spatial partition system or implement a per-frame spatial hash to reduce the number of distance checks needed.
    void foreachEntityInRadius( const Vec3& center, float radius, EntityCallback cb, void* ctx ) {
        foreachEntityRecord( [&]( EntityRecord* rec ) {
            auto entity = getEntityPointer( rec );
            if ( !entity ) return;
            if ( ( entity->pos - center ).length() <= radius ) {
                cb( ctx, entity );
            }
        } );
    }

}
