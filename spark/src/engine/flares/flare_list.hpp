#pragma once

#include "../common.hpp"
#include "types.hpp"
#include <type_traits>

namespace Engine {
    bool isFlareManagerLoaded();
    FlareManager* getFlareManager();
    FlareEntry* getFlareEntry( uint32_t flareHandle );

    // Raw C-ABI-safe callback (function pointer + context) — safe across a DLL boundary,
    // unlike std::function (implementation-defined layout/allocator).
    using FlareEntryCallback = void(*)( void* ctx, FlareEntry* entry, uint32_t handle );
    void foreachFlareEntry( FlareEntryCallback cb, void* ctx );

    // Header-only convenience wrapper — instantiated per call site, so ordinary capturing
    // lambdas keep working. Only the trampoline (a plain function pointer) crosses into
    // Engine's actual implementation, never the lambda/closure itself.
    template<typename Fn>
    inline void foreachFlareEntry( Fn&& cb ) {
        foreachFlareEntry(
            +[]( void* ctx, FlareEntry* entry, uint32_t handle ) { (*static_cast<std::remove_reference_t<Fn>*>(ctx))( entry, handle ); },
            &cb
        );
    }
}
