#define SPARK_EXPORTS
#include "spark/hook/Hooks.hpp"

// The one real, exported explicit instantiation for each hook's storage
// (bus/original/base). Every other translation unit only sees the `extern template`
// declarations in Hooks.hpp and imports this instance rather than duplicating it.
namespace Spark {
    #define HOOK(Name, Ret, Offset, ...) template struct Hook<Offset, Ret, __VA_ARGS__>;
    #include "spark/hook/HookTable.hpp"
    #undef HOOK
}
