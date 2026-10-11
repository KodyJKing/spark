#include "Memory.hpp"
#include <vector>

namespace Memory {

    bool isAllocated( uintptr_t address ) {
        MEMORY_BASIC_INFORMATION mbi;
        if ( !VirtualQuery( (LPCVOID) address, &mbi, sizeof( mbi ) ) )
            return false;
        return mbi.State == MEM_COMMIT;
    }
    
    bool isAllocatedFast( uintptr_t address ) {
        uintptr_t dummy;
        static HANDLE proc = GetCurrentProcess();
        return ReadProcessMemory(proc, (LPCVOID)address, &dummy, sizeof(dummy), NULL);
    }

    bool isAllocatedFast( void* ptr ) {
        return isAllocatedFast( (uintptr_t) ptr );
    }

    bool isAllocated( void* ptr ) {
        return isAllocated( (uintptr_t) ptr );
    }

}