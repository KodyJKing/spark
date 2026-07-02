#include "Scripting.hpp"
#include "engine/common.hpp"
#include <cstdint>
#include <cstring>

// _consoleSubmit — 7fff49b51470
// Normalizes raw input, compiles, and executes a HaloScript expression.
static constexpr uintptr_t consoleSubmitOffset       = 0xB21470U;

// Individual pipeline stages used by submitEval.
// Offsets confirmed via ghidra_offset.py.
static constexpr uintptr_t normalizeConsoleInputOffset = 0xB21278U; // _normalizeConsoleInput
static constexpr uintptr_t hsBeginCompileOffset        = 0xBF854CU; // _hsBeginCompile
static constexpr uintptr_t hsCompileExpressionOffset   = 0xBF86E4U; // _hsCompileExpression
static constexpr uintptr_t hsExecuteExpressionOffset   = 0xACAD70U; // _hsExecuteExpression
static constexpr uintptr_t hsEndCompileOffset          = 0xBF8628U; // _hsEndCompile

static constexpr uintptr_t lookupSymbolOffset          = 0xB205C8U;
static constexpr uintptr_t readSymbolOffset            = 0xACC0C4U;

namespace Engine::Scripting {

    void submit(const char* input) {
        typedef void (*consoleSubmit_t)(const char*);
        auto fn = (consoleSubmit_t)(Engine::dllBase() + consoleSubmitOffset);
        fn(input);
    }

    GlobalSearchResult lookupGlobal(const char* name) {
        typedef GlobalSearchResult (*lookupSymbol_t)(const char*);
        auto fn = (lookupSymbol_t)(Engine::dllBase() + lookupSymbolOffset);
        return fn(name);
    }
    
    uint32_t readGlobal(int16_t index) {
        typedef uint32_t (*readSymbol_t)(int16_t);
        auto fn = (readSymbol_t)(Engine::dllBase() + readSymbolOffset);
        return fn(index);
    }
    
    uint32_t readGlobal(const char* name) {
        GlobalSearchResult sym = lookupGlobal(name);
        if (sym.index == -1) return 0xFFFFFFFFu;
        return readGlobal(sym.index);
    }

}
