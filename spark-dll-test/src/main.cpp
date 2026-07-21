#include <eh.h>
#include <iomanip>
#include <iostream>

#include "Halo1Module.hpp"
#include "TestRunner.hpp"

std::vector<TestCase>& Halo1TestRegistry() {
    static std::vector<TestCase> tests;
    return tests;
}

namespace {

// Compiled with /EHa (see premake5.lua) so that a bad hypothesis about a
// function's "purity" -- e.g. it turns out to dereference an uninitialized
// global -- surfaces as a normal FAIL with a message instead of taking down
// the whole harness.
void SehTranslator(unsigned int code, EXCEPTION_POINTERS* ep) {
    char buf[64];
    sprintf_s(buf, "structured exception 0x%08X at 0x%p", code, ep->ExceptionRecord->ExceptionAddress);
    throw std::runtime_error(buf);
}

}  // namespace

int main() {
    _set_se_translator(SehTranslator);

    std::cout << "Loading halo1.dll...\n";
    Halo1Module halo1;
    std::cout << "Loaded at base 0x" << std::hex << halo1.base() << std::dec << "\n\n";

    int passed = 0;
    int failed = 0;

    for (auto& test : Halo1TestRegistry()) {
        std::cout << "[ RUN  ] " << test.name << "\n";

        bool ok = false;
        try {
            ok = test.run(halo1);
        } catch (const std::exception& e) {
            std::cout << "  threw: " << e.what() << "\n";
        }

        std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ") << test.name << "\n\n";
        ok ? ++passed : ++failed;
    }

    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
