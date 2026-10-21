#include "Memory.hpp"
#include <vector>

namespace Memory {

    bool isAllocated( uintptr_t address ) {
        // Keep an eye on this.
        return isAllocatedFast(address);
        // MEMORY_BASIC_INFORMATION mbi;
        // if ( !VirtualQuery( (LPCVOID) address, &mbi, sizeof( mbi ) ) )
        //     return false;
        // return mbi.State == MEM_COMMIT;
    }
    
    bool isAllocated( void* ptr ) {
        return isAllocated( (uintptr_t) ptr );
    }

    bool isAllocatedFast( uintptr_t address ) {
        uintptr_t dummy;
        return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)address, &dummy, sizeof(dummy), NULL);
    }

    bool isAllocatedFast( void* ptr ) {
        return isAllocated( (uintptr_t) ptr );
    }

}
