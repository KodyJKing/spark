#include "map.hpp"
#include "common.hpp"
#include "memory/Memory.hpp"
#include "memory/Allocator.hpp"

namespace Engine {

    static const uintptr_t tagHeaderBaseOffset = 0x2D9CE10U;
    static const uintptr_t tagPointerOffsetOffset = 0x2EA3410U;

    uint64_t tagHeaderBase() {
        return *(uint64_t*) ( dllBase() + tagHeaderBaseOffset );
    }

    uint64_t tagPointerOffset() {
        return *(uint64_t*) ( dllBase() + tagPointerOffsetOffset );
    }

    uint64_t tagDataBase() {
        uint64_t tagHeaderBase = *(uint64_t*) ( dllBase() + tagHeaderBaseOffset );
        uint64_t tagPointerOffset = *(uint64_t*) ( dllBase() + tagPointerOffsetOffset );
        return tagHeaderBase - tagPointerOffset;
    }

    // Misnomer, rename to translateTagDataPointer when not feeling lazy.
    uint64_t translateMapAddress( uint32_t address ) {
        uint64_t relocatedMapBase = *(uint64_t*) ( dllBase() + tagHeaderBaseOffset );
        uint64_t mapBase = *(uint64_t*) ( dllBase() + tagPointerOffsetOffset );
        return address + ( relocatedMapBase - mapBase );
    }
    
    // Misnomer, rename to translateToTagDataPointer when not feeling lazy.
    uint32_t translateToMapAddress( uint64_t absoluteAddress ) {
        uint64_t relocatedMapBase = *(uint64_t*) ( dllBase() + tagHeaderBaseOffset );
        uint64_t mapBase = *(uint64_t*) ( dllBase() + tagPointerOffsetOffset );
        return (uint32_t) ( absoluteAddress - ( relocatedMapBase - mapBase ) );
    }

    // Misnomer, rename to canTranslateToTagDataPointer when not feeling lazy.
    bool canTranslateToMapAddress( uint64_t absoluteAddress ) {
        auto relativeAddress = translateToMapAddress( absoluteAddress );
        auto absoluteAddressCheck = translateMapAddress( relativeAddress );
        return absoluteAddressCheck == absoluteAddress;
    }

    void* allocateMapMemory(size_t size) {
        return (void*) Memory::allocBlockNear(tagDataBase(), size);
    }

    void freeMapMemory(void* address) {
        Memory::freeBlock((uintptr_t) address);
    }

    char* getMapName() {
        MapHeader* header = getMapHeader();
        if ( !header ) return nullptr;
        return header->mapName;
    }

    bool checkMapHeader(MapHeader* header) {
        if (!header) {
            // std::cout << "Error: header is null" << std::endl;
            return false;
        }
        if ( !Memory::isAllocated( (uintptr_t) header ) ) {
            // std::cout << "Error: header is not allocated" << std::endl;
            return false;
        }
        if (header->magicHeader != 1751474532) {
            // std::cout << "Error: magicHeader is not 1751474532" << std::endl;
            return false;
        }
        if (header->magicFooter != 1718579060) {
            // std::cout << "Error: magicFooter is not 1718579060" << std::endl;
            // std::cout << offsetof(MapHeader, magicFooter) << std::endl;
            return false;
        }
        return true;
    }

    MapHeader* getMapHeader() {
        MapHeader* result = (MapHeader*) ( dllBase() + 0x2B22744U );
        if ( !checkMapHeader( result ) )
            return nullptr;
        return result;
    }

    bool isOnMap( const char* mapName ) {
        auto actualMapName = getMapName();
        if ( !actualMapName )
            return false;
        return strncmp( mapName, actualMapName, strnlen( mapName, 32 ) ) == 0;
    }

    bool isMapLoaded() {
        auto header = getMapHeader();
        return header && Memory::isAllocated( (uintptr_t) header ) && checkMapHeader( header );
    }

}