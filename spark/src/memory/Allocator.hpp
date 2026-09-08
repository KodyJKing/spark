// A block allocator with control over the allocation address and protection flags. 

#pragma once

#include <stdint.h>
#include <Windows.h>

namespace Memory {

    enum ScanMode {
        ScanMode_Foreward,
        ScanMode_Backward,
        ScanMode_Both
    };

    // Allocate a block of memory near a given address, will use an existing free block if possible.
    // Will try to allocate within a 32-bit offset of `nearAddress`.
    uintptr_t allocBlockNear(uintptr_t nearAddress, size_t size, DWORD flProtect = PAGE_READWRITE, ScanMode scanMode = ScanMode_Foreward);

    bool freeBlock(uintptr_t address );

    // Frees all blocks allocated by Spark.
    bool freeAllBlocks();

}
