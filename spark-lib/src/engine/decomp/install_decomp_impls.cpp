#include "install_decomp_impls.hpp"
#include "impl/index.hpp"
#include "MinHook.h"

namespace Engine::Decomp {

    void install(uintptr_t halo1) {
        /*
            Installation X-Macro declaring redirects into decomp functions:
                Hook(Impl, Offset) ->
                    MH_CreateHook((void*)(halo1 + Offset), &Impl, nullptr);
        */
       #define HOOK(Impl, Offset) MH_CreateHook((void*)(halo1 + Offset), &Impl, nullptr);
       #include "decomp_defs.hpp"
       #undef HOOK

       /*
            Enable X-Macro declaring enabling of decomp function hooks:
                Hook(Impl, Offset) ->
                    MH_EnableHook((void*)(halo1 + Offset));
       */
        #define HOOK(Impl, Offset) MH_EnableHook((void*)(halo1 + Offset));
        #include "decomp_defs.hpp"
        #undef HOOK 
    }

    void uninstall(uintptr_t halo1) {
        /*
            Uninstallation X-Macro declaring redirects into decomp functions:
                Hook(Impl, Offset) ->
                    MH_RemoveHook((void*)(halo1 + Offset));
        */
       #define HOOK(Impl, Offset) MH_RemoveHook((void*)(halo1 + Offset));
       #include "decomp_defs.hpp"
       #undef HOOK
    }

}
