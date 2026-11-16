#pragma once

#include <functional>
#include <Windows.h>

namespace Memory {

    struct ThreadState {
        CONTEXT context{};
        HANDLE thread = nullptr;

        // The trapped operand, before the instruction executes. Size is 0 if its extent
        // cannot be decoded (reported to the debugger). Standalone context decoding throws.
        char* operandAddress();
        size_t operandSize();

    private:
        friend struct MemoryBreakpointHandler;
        char* m_operandAddress = nullptr;
        size_t m_operandSize = 0;
    };

    // Watches reads, writes and execution. Identical ranges replace their callback.
    // Callbacks run synchronously on the faulting thread; context edits are applied on return.
    // The thread handle is borrowed, callbacks must not throw, and recursive callbacks are suppressed.
    // Invalid ranges/protections throw; Win32 failures throw std::system_error.
    //
    // PAGE_GUARD is page-wide: use dedicated committed pages, not stacks, heap metadata,
    // or the breakpoint/handler code itself. Existing guards and inaccessible pages are rejected.
    // Other threads can miss accesses while a page is temporarily unguarded. Operand decoding
    // does not expand REP/gather/scatter operations into individual accesses.
    // Serialize registration/removal with accesses by other threads.
    // The VEH is installed on first use and removed after the last watch and pending
    // single step. Remove all watches before freeing memory;
    // do not unload the module while watched threads or pending single steps are active.
    void setMemoryBreakpoint(void* address, size_t size, std::function<void(ThreadState&)> callback);
    // Removes an exact range; removing an absent range is a no-op.
    void removeMemoryBreakpoint(void* address, size_t size);
}
