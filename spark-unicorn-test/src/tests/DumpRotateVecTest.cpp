#include <cstdint>
#include <iostream>

#include "TestRunner.hpp"
#include "UnicornEngine.hpp"
#include "Win64Call.hpp"
#include "MinidumpLoader.hpp"
#include "SharedDumpFixture.hpp"

#include "engine/math.hpp"

namespace {

constexpr uintptr_t kRotateVecOffset = 0xBA3004;
using RotateVecFn = Vec3*(Engine::Transform*, Vec3*, Vec3*);

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
//
// SHARED-SAFE: confirmed via Ghidra (2026-07-19) -- rotateVec is pure math
// with zero global reads/writes (see reversing/notes/WorldBones.md
// "Transform math family"), so it's safe to run against the fixture engine
// shared with other tests.
UNICORN_TEST(Dump_RotateVec_AxisSwapMatrix_SwapsXAndY) {
    auto& fixture = SharedDumpFixture::instance();
    MinidumpLoader& dump = fixture.dump();
    UnicornEngine& engine = fixture.engine();

    uint64_t halo1Base = dump.moduleBase("halo1.dll");
    uint64_t rotateVecAddr = halo1Base + kRotateVecOffset;

    uint64_t scratchBase = fixture.allocateScratchSlot();
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
