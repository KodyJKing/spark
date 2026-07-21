#include <cstdint>
#include <cstring>
#include <iostream>

#include "TestRunner.hpp"
#include "UnicornEngine.hpp"
#include "Win64Call.hpp"

namespace {

constexpr uint64_t kCodeBase = 0x00400000;
constexpr uint64_t kStackBase = 0x7FFF0000;
constexpr uint64_t kStackSize = 0x10000;
constexpr uint64_t kReturnSentinel = 0x00410000;  // arbitrary mapped-but-never-executed address

}  // namespace

// Validates the whole pipeline -- map code, map a stack, set args per the
// Win64 fastcall convention, run, read RAX -- against a hand-assembled
// function rather than a real captured dump. No dump files required; this
// should always pass regardless of whether any dumps have been captured
// yet, so it's a good sanity check that the Unicorn build/link/harness
// itself is working before writing real halo1.dll-derived tests.
UNICORN_TEST(Smoke_AddTwoIntegers_ReturnsSum) {
    UnicornEngine engine;

    // x86-64: mov eax, ecx ; add eax, edx ; ret
    const uint8_t code[] = { 0x89, 0xC8, 0x01, 0xD0, 0xC3 };
    engine.mapAndWrite(kCodeBase, code, sizeof(code), UC_PROT_READ | UC_PROT_EXEC);

    UnicornEngine::check(
        uc_mem_map(engine.handle(), kStackBase, kStackSize, UC_PROT_READ | UC_PROT_WRITE),
        "uc_mem_map(stack)");

    // Never fetched from -- just needs to be a valid mapped address for
    // uc_emu_start's `until` bound.
    UnicornEngine::check(
        uc_mem_map(engine.handle(), kReturnSentinel, 0x1000, UC_PROT_READ | UC_PROT_EXEC),
        "uc_mem_map(sentinel)");

    Win64Call call{ engine, kStackBase + kStackSize - 0x1000, kReturnSentinel };
    uint64_t result = call.call(kCodeBase, /*a0=*/10, /*a1=*/32);

    if (result != 42) {
        std::cout << "  expected 42, got " << result << "\n";
        return false;
    }
    return true;
}
