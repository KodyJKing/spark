#include "Scripting.hpp"
#include "engine/common.hpp"

// _consoleSubmit — 7fff49b51470
// Normalizes raw input, compiles, and executes a HaloScript expression.
static constexpr uintptr_t consoleSubmitOffset = 0xB21470U;

namespace Engine::Scripting {

    void submit(const char* input) {
        typedef void (*consoleSubmit_t)(const char*);
        auto fn = (consoleSubmit_t)(Engine::dllBase() + consoleSubmitOffset);
        fn(input);
    }

}
