#include "Allocator.hpp"

#include "Windows.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>

#define __DEBUG_ALLOCATOR__

namespace Memory {

    // === Helper functions ==================

    uintptr_t absDiff( uintptr_t a, uintptr_t b ) {
        if ( a > b ) return a - b;
        return b - a;
    }

    uintptr_t roundUpToPowerOfTwo(uintptr_t value) {
        uintptr_t result = 1;
        while (result < value)
            result *= 2;
        return result;
    }

    // === findFreePageNear implementation ===

    const size_t _MEM_INFO_SIZE = sizeof( MEMORY_BASIC_INFORMATION );

    inline size_t getPageSize() {
        SYSTEM_INFO sysInfo;
        GetSystemInfo( &sysInfo );
        return sysInfo.dwPageSize;
    }

    inline size_t roundUpToPageSize( size_t bytes ) {
        auto pageSize = getPageSize();
        auto pages = bytes / pageSize;
        if ( bytes % pageSize > 0 )
            pages++;
        return pages * pageSize;
    }

    inline uintptr_t roundDown( uintptr_t x, uintptr_t granularity ) {
        return ( x / granularity ) * granularity;
    }

    inline uintptr_t roundUp( uintptr_t x, uintptr_t granularity ) {
        auto result = ( x / granularity ) * granularity;
        if ( x % granularity > 0 )
            result += granularity;
        return result;
    }

    uintptr_t scanForFreePage( uintptr_t address, size_t minSize, int direction ) {
        MEMORY_BASIC_INFORMATION memInfo;
        SYSTEM_INFO sysInfo;

        GetSystemInfo( &sysInfo );

        uintptr_t currentAddress = roundUp( address, sysInfo.dwAllocationGranularity );

        while ( true ) {

            auto resultBytes = VirtualQuery( (LPVOID) currentAddress, &memInfo, _MEM_INFO_SIZE );
            if ( resultBytes != _MEM_INFO_SIZE )
                return 0;

            if ( ( memInfo.State == MEM_FREE ) && ( memInfo.RegionSize >= minSize ) )
                return (uintptr_t) memInfo.BaseAddress;

            if ( direction > 0 )
                currentAddress = roundUp(
                    (uintptr_t) memInfo.BaseAddress + memInfo.RegionSize,
                    sysInfo.dwAllocationGranularity
                );
            else
                currentAddress = roundDown(
                    (uintptr_t) memInfo.BaseAddress - 1,
                    sysInfo.dwAllocationGranularity
                );

            intptr_t currentOffset = currentAddress - address;
            if ( currentOffset > MAXINT32 || currentOffset < MININT32 )
                return 0;

        }
    }

    uintptr_t findFreePageNear( uintptr_t address, size_t minSize, ScanMode scanMode = ScanMode_Both ) {
        auto backAddress = (scanMode == ScanMode_Backward || scanMode == ScanMode_Both) ? scanForFreePage(address, minSize, -1 ) : 0;
        auto forwardAddress = (scanMode == ScanMode_Foreward || scanMode == ScanMode_Both) ? scanForFreePage(address, minSize, 1 ) : 0;

        auto backDist = address > backAddress ? address - backAddress : backAddress - address;
        auto forwardDist = address > forwardAddress ? address - forwardAddress : forwardAddress - address;

        return backDist < forwardDist ? backAddress : forwardAddress;
    }

    // =======================================
    
    struct Region {
        uintptr_t address;
        size_t size;

        Region() : address( 0 ), size( 0 ) {}
        Region( uintptr_t address, size_t size ) : address( address ), size( size ) {}
    };

    std::vector<Region> allocatedRegions;

    struct Block {
        uintptr_t address;
        size_t size;
        DWORD flProtect;

        Block() : address( 0 ), size( 0 ), flProtect( 0 ) {}
        Block( uintptr_t address, size_t size, DWORD flProtect ) : address( address ), size( size ), flProtect( flProtect ) {}
    };

    /// @brief A group of blocks with the same size and protection flags.
    struct BlockGroup {
        size_t size;
        DWORD flProtect;

        BlockGroup() : size( 0 ), flProtect( 0 ) {}
        BlockGroup( size_t size, DWORD flProtect ) : size( size ), flProtect( flProtect ) {}

        std::map<uintptr_t, Block> blocks;
        std::vector<Block> freeBlocks;

        uintptr_t allocateNear( uintptr_t preferredAddress, ScanMode scanMode ) {
            // Scan for a free block within 32-bit offset of preferredAddress.
            // Iterate backwards to get the highest address.
            for (auto it = freeBlocks.rbegin(); it != freeBlocks.rend(); ++it) {
                if (absDiff(it->address, preferredAddress) < 0x100000000) {
                    Block block = *it;
                    freeBlocks.erase((++it).base());
                    blocks[block.address] = block;
                    return block.address;
                }
            }

            // No free block within 32-bit offset of preferredAddress.

            // Try to allocate a block near the preferred address.
            if (allocateBlocks(preferredAddress, scanMode)) {
                Block block = freeBlocks.back();
                freeBlocks.pop_back();
                blocks[block.address] = block;
                return block.address;
            }

            return 0;
        }

        uintptr_t allocate( uintptr_t preferredAddress, ScanMode scanMode ) {
            if (preferredAddress)
                return allocateNear(preferredAddress, scanMode);

            // Try to allocate a block if there are no free blocks.
            if ( freeBlocks.size() == 0 && !allocateBlocks(0, scanMode) )
                    return 0;

            Block block = freeBlocks.back();
            freeBlocks.pop_back();
            blocks[block.address] = block;
            return block.address;
        }

        bool free( uintptr_t address ) {
            auto it = blocks.find( address );
            if ( it == blocks.end() ) {
                return false;
            }

            auto block = it->second;

            freeBlocks.push_back( block );
            blocks.erase( it );

            #ifdef __DEBUG_ALLOCATOR__
            std::cout << "A block of size " << size << "b was freed at " << (void*)address << ".\n";
            #endif

            return true;
        }

        // Allocates more blocks with VirtualAllocEx.
        // Splits the returned region into blocks.
        bool allocateBlocks(uintptr_t preferredAddress, ScanMode scanMode) {
            if (preferredAddress != 0)
                preferredAddress = findFreePageNear(preferredAddress, size, scanMode);

            SYSTEM_INFO si;
            GetSystemInfo( &si );
            size_t granularity = si.dwAllocationGranularity;

            size_t requestedRegionSize = size;
            if (requestedRegionSize < granularity)
                requestedRegionSize = granularity;

            LPVOID lpPreferredAddress = (LPVOID)preferredAddress;
            uintptr_t address = (uintptr_t)VirtualAlloc( lpPreferredAddress, requestedRegionSize, MEM_COMMIT | MEM_RESERVE, flProtect );
            if ( address == 0 ) {
                return false;
            }

            allocatedRegions.push_back( Region( address, requestedRegionSize ) );

            // Query the region size.
            MEMORY_BASIC_INFORMATION mbi;
            VirtualQuery((LPCVOID)address, &mbi, sizeof( MEMORY_BASIC_INFORMATION ) );
            size_t regionSize = mbi.RegionSize;

            #ifdef __DEBUG_ALLOCATOR__
            std::cout << "A region of size " << regionSize << "b was allocated at " << (void*)address << ".\n";
            #endif

            // Split the region into blocks.
            for ( uintptr_t i = address; i < address + regionSize; i += size ) {
                blocks[i] = Block( i, size, flProtect );
                freeBlocks.push_back( blocks[i] );
            }

            #ifdef __DEBUG_ALLOCATOR__
            auto numBlocks = regionSize / size;
            std::cout << "The region was split into " << numBlocks << " blocks of size " << size << "b.\n\n";
            #endif

            return true;
        }
    };

    using BlockKey = std::tuple<size_t, DWORD>; // size, protection

    std::map<BlockKey, BlockGroup> blockGroups;

    BlockKey makeKey( size_t size, DWORD flProtect ) {
        return std::make_tuple( size, flProtect );
    }

    // === Public API ========================

    uintptr_t allocBlockNear( uintptr_t nearAddress, size_t size, DWORD flProtect, ScanMode scanMode ) {
        // The public API enforces power of two block sizes.
        size = roundUpToPowerOfTwo( size );
        
        BlockKey key = makeKey( size, flProtect );
        auto it = blockGroups.find( key );
        if ( it == blockGroups.end() ) {
            blockGroups[key] = BlockGroup( size, flProtect );
            it = blockGroups.find( key );
        }

        return it->second.allocateNear( nearAddress, scanMode );
    }

    bool freeBlock( uintptr_t address ) {
        // Loop over all groups and try to free the block.
        for (auto it = blockGroups.begin(); it != blockGroups.end(); ++it) {
            auto group = it->second;
            if (it->second.free(address))
                return true;
        }
        return false;
    }

    bool freeAllBlocks() {
        bool success = true;
        for (auto it = allocatedRegions.begin(); it != allocatedRegions.end(); ++it) {
            auto region = *it;
            if (!VirtualFree((LPVOID)region.address, 0, MEM_RELEASE)) {
                success = false;
                continue;
            }
            #ifdef __DEBUG_ALLOCATOR__
            std::cout << "A region of size " << region.size << "b was freed at " << (void*)region.address << ".\n";
            #endif
        }
        
        allocatedRegions.clear();
        blockGroups.clear();

        return success;
    }

}
