#include <cstdint>
#include <iostream>

#include "TestRunner.hpp"
#include "UnicornEngine.hpp"
#include "Win64Call.hpp"
#include "MinidumpLoader.hpp"

#include "engine/math.hpp"

namespace {

constexpr uintptr_t kRotateVecOffset = 0xBA3004;
using RotateVecFn = Vec3*(Engine::Transform*, Vec3*, Vec3*);

constexpr const char* kDumpPath =
    R"(C:\code\projects\hacking\smc64\smc64-dlltest-unicorn\dumps\MCC-Win64-Shipping.DMP)";

}  // namespace

// Sanity check for the whole "load a real process dump, call into it via
// Unicorn" pipeline. Re-runs one of smc64-dlltest's already-confirmed pure
// tests (RotateVec_AxisSwapMatrix_SwapsXAndY from TransformTests.cpp), but
// against halo1.dll's code as captured in a full memory dump instead of a
// freshly LoadLibrary'd copy. Matching that same result here validates
// module-base resolution, memory loading, and the Win64 calling-convention
// setup all at once, using a function whose expected output we already know
// -- before writing any *new* globals-dependent tests that this suite
// actually exists for.
UNICORN_TEST(Dump_RotateVec_AxisSwapMatrix_SwapsXAndY) {
    MinidumpLoader dump(kDumpPath);
    UnicornEngine engine;
    // rotateVec has zero global dependencies, so we only need halo1.dll's
    // own code mapped -- not the whole 5GB process. Loading everything trips
    // QEMU/Unicorn's hard cap of ~4096 distinct memory-region "sections"
    // (a real full-process dump has far more individual VM regions than
    // that); loadModule() scopes the mapping to just this module's image.
    dump.loadModule(engine, "halo1.dll");

    uint64_t halo1Base = dump.moduleBase("halo1.dll");
    uint64_t rotateVecAddr = halo1Base + kRotateVecOffset;

    // Scratch region for our synthetic stack + argument structs + return
    // sentinel. Picked from addresses far outside where a normal Win64
    // process allocates (module images / heaps / stacks all live well below
    // these), and verified free by probing -- we can't know for certain
    // what's unused in someone else's captured address space otherwise.
    uint64_t scratchBase = 0;
    for (uint64_t candidate : { 0x0000700000000000ULL, 0x0000600000000000ULL,
                                0x0000500000000000ULL, 0x0000200000000000ULL }) {
        if (engine.tryMap(candidate, 0x20000, UC_PROT_ALL)) {
            scratchBase = candidate;
            break;
        }
    }
    if (scratchBase == 0) {
        std::cout << "  could not find a free scratch region in the dumped address space\n";
        return false;
    }

    uint64_t stackTop = scratchBase + 0x10000;
    uint64_t sentinel = scratchBase + 0x11000;
    uint64_t argsBase = scratchBase + 0x12000;

    // Same axis-swap transform as smc64-dlltest's RotateVec_AxisSwapMatrix
    // test: x/y basis columns swapped, so local X maps to world Y and vice
    // versa.
    Engine::Transform swapXY{};
    swapXY.w = 1.0f;
    swapXY.x = { 0, 1, 0 };
    swapXY.y = { 1, 0, 0 };
    swapXY.z = { 0, 0, 1 };
    swapXY.pos = { 0, 0, 0 };

    Vec3 in{ 3.0f, -2.0f, 5.0f };
    Vec3 out{};

    uint64_t trAddr = argsBase;
    uint64_t inAddr = argsBase + 0x100;
    uint64_t outAddr = argsBase + 0x200;

    engine.memWrite(trAddr, &swapXY, sizeof(swapXY));
    engine.memWrite(inAddr, &in, sizeof(in));
    engine.memWrite(outAddr, &out, sizeof(out));

    Win64Call call{ engine, stackTop, sentinel };
    call.call(rotateVecAddr, trAddr, inAddr, outAddr);

    engine.memRead(outAddr, &out, sizeof(out));

    Vec3 expected{ in.y, in.x, in.z };  // (-2, 3, 5)
    bool ok = out.x == expected.x && out.y == expected.y && out.z == expected.z;
    if (!ok) {
        std::cout << "  halo1.dll base in dump: 0x" << std::hex << halo1Base << std::dec << "\n";
        std::cout << "  expected (" << expected.x << ", " << expected.y << ", " << expected.z << ")"
                   << ", got (" << out.x << ", " << out.y << ", " << out.z << ")\n";
    }
    return ok;
}
