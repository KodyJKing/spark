#include "flare_list.hpp"
#include "memory/Memory.hpp"

namespace Engine {

    FlareManager* getFlareManager() { return *(FlareManager**) ( dllBase() + 0x29E0900 ); }
    bool isFlareManagerLoaded() { return Memory::isAllocated( (uintptr_t) getFlareManager() ); }

    FlareEntry* getFlareEntry( uint32_t flareHandle ) {
        if ( flareHandle == NULL_HANDLE ) return nullptr;
        auto mgr = getFlareManager();
        if ( !mgr ) return nullptr;
        uint16_t index = flareHandle & 0xFFFF;
        if ( index >= (uint16_t) mgr->capacity ) return nullptr;
        auto entry = mgr->entryAtIndex( index );
        if ( entry->generation == 0 ) return nullptr;
        return entry;
    }

    void foreachFlareEntry( std::function<void( FlareEntry*, uint32_t handle )> cb ) {
        auto mgr = getFlareManager();
        if ( !mgr ) return;
        for ( int16_t i = 0; i < mgr->capacity; i++ ) {
            auto entry = mgr->entryAtIndex( i );
            if ( entry->generation == 0 ) continue;
            uint32_t handle = ( (uint32_t) (uint16_t) entry->generation << 16 ) | (uint16_t) i;
            cb( entry, handle );
        }
    }

}
