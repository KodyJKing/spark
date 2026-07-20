#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#include "TestRunner.hpp"
#include "UnicornEngine.hpp"
#include "Win64Call.hpp"
#include "MinidumpLoader.hpp"
#include "SharedDumpFixture.hpp"

#include "engine/math.hpp"

// Diagnostic (not a correctness check): decides whether a Unicorn test could
// be "promoted" to a plain smc64-dlltest (real LoadLibrary'd DLL, no
// dump/emulation) by recording every memory access a call makes and
// classifying each one as:
//   - inside halo1.dll's own image        -> static/const data, a fresh
//                                            LoadLibrary already has this,
//                                            nothing to synthesize
//   - inside this test's own scratch slot -> expected (args/stack/output),
//                                            already known/synthesized
//   - anything else                       -> dump-backed heap/global state
//                                            (entity pool, tag array, BSP
//                                            data, ...) that a plain
//                                            smc64-dlltest would need to
//                                            fake somehow
//
// A test is "promotable" only if that third bucket is empty. Returns that
// verdict as the test's pass/fail result so it shows up directly in the
// suite summary; -Filter Trace to run just these and read the printed
// per-range breakdown.
namespace {

struct Bucket {
    const char* label;
    uint64_t bytes = 0;
    std::vector<std::pair<uint64_t, uint64_t>> ranges;  // [start, end)
};

void addToRange(std::vector<std::pair<uint64_t, uint64_t>>& ranges, uint64_t addr, uint64_t size) {
    ranges.emplace_back(addr, addr + size);
}

void coalesce(std::vector<std::pair<uint64_t, uint64_t>>& ranges) {
    if (ranges.empty()) return;
    std::sort(ranges.begin(), ranges.end());
    std::vector<std::pair<uint64_t, uint64_t>> merged{ ranges.front() };
    for (size_t i = 1; i < ranges.size(); ++i) {
        auto& last = merged.back();
        if (ranges[i].first <= last.second) {
            last.second = (std::max)(last.second, ranges[i].second);
        } else {
            merged.push_back(ranges[i]);
        }
    }
    ranges = std::move(merged);
}

// Classifies `records` against the module image range and one scratch-slot
// range, prints a summary, and returns true iff every access falls inside
// the module image or the scratch slot (i.e. nothing dump-backed was
// touched -- the test is promotable).
bool classifyAndReport(const std::vector<MemoryAccessRecord>& records,
                        uint64_t imageBase, uint64_t imageEnd,
                        uint64_t scratchBase, uint64_t scratchEnd) {
    Bucket inImage{ "module image (already correct in a fresh LoadLibrary)" };
    Bucket inScratch{ "test scratch (own args/stack/output)" };
    Bucket other{ "OTHER -- dump-backed heap/global state" };

    for (auto& r : records) {
        if (r.address >= imageBase && r.address < imageEnd) {
            inImage.bytes += r.size;
            addToRange(inImage.ranges, r.address, r.size);
        } else if (r.address >= scratchBase && r.address < scratchEnd) {
            inScratch.bytes += r.size;
            addToRange(inScratch.ranges, r.address, r.size);
        } else {
            other.bytes += r.size;
            addToRange(other.ranges, r.address, r.size);
        }
    }

    for (Bucket* b : { &inImage, &inScratch, &other }) {
        coalesce(b->ranges);
        std::cout << "  " << b->label << ": " << b->bytes << " bytes across "
                  << b->ranges.size() << " range(s)\n";
        if (b == &other) {
            for (auto& [start, end] : b->ranges) {
                std::cout << "    0x" << std::hex << start << " .. 0x" << end
                           << std::dec << " (" << (end - start) << " bytes)\n";
            }
        }
    }

    bool promotable = other.bytes == 0;
    std::cout << "  verdict: " << (promotable ? "PROMOTABLE to smc64-dlltest"
                                               : "NOT promotable as-is (touches dump-backed state)")
               << "\n";
    return promotable;
}

}  // namespace

UNICORN_TEST(Trace_RotateVec_ClassifyMemoryAccesses) {
    auto& fixture = SharedDumpFixture::instance();
    MinidumpLoader& dump = fixture.dump();
    UnicornEngine& engine = fixture.engine();

    uint64_t halo1Base = dump.moduleBase("halo1.dll");
    uint64_t halo1End = halo1Base + dump.moduleSize("halo1.dll");
    uint64_t rotateVecAddr = halo1Base + 0xBA3004;

    uint64_t scratchBase = fixture.allocateScratchSlot();
    uint64_t scratchEnd = scratchBase + SharedDumpFixture::kSlotSize;
    uint64_t stackTop = scratchBase + 0x10000;
    uint64_t sentinel = scratchBase + 0x11000;
    uint64_t argsBase = scratchBase + 0x12000;

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
    auto records = engine.traceMemoryAccesses([&] {
        call.call(rotateVecAddr, trAddr, inAddr, outAddr);
    });

    std::cout << "  " << records.size() << " memory accesses recorded\n";
    return classifyAndReport(records, halo1Base, halo1End, scratchBase, scratchEnd);
}

UNICORN_TEST(Trace_LineTestVsEntityModel_ClassifyMemoryAccesses) {
    auto& fixture = SharedDumpFixture::instance();
    MinidumpLoader& dump = fixture.dump();
    UnicornEngine& engine = fixture.engine();

    uint64_t halo1Base = dump.moduleBase("halo1.dll");
    uint64_t halo1End = halo1Base + dump.moduleSize("halo1.dll");
    uint64_t funcAddr = halo1Base + 0xB91C0C;

    uint64_t scratchBase = fixture.allocateScratchSlot();
    uint64_t scratchEnd = scratchBase + SharedDumpFixture::kSlotSize;
    uint64_t stackTop = scratchBase + 0x10000;
    uint64_t sentinel = scratchBase + 0x11000;
    uint64_t argsBase = scratchBase + 0x12000;

    uint64_t queryAddr = argsBase;
    uint64_t originAddr = argsBase + 0x100;
    uint64_t dirAddr = argsBase + 0x200;
    uint64_t resultAddr = argsBase + 0x300;

    struct Vec3f { float x, y, z; };
#pragma pack(push, 1)
    struct RawCollisionQuery {
        int16_t queryType;
        uint8_t pad_0x02[0x36];
        uint32_t entityHandle;
        uint8_t pad_0x3c[8];
    };
#pragma pack(pop)

    RawCollisionQuery query{};
    query.queryType = 3;
    query.entityHandle = 0xE64503D6;
    engine.memWrite(queryAddr, &query, sizeof(query));

    Vec3f origin{ 23.066f, -17.000f, 89.216f + 3.0f };
    Vec3f direction{ 0.0f, 0.0f, -6.0f };
    engine.memWrite(originAddr, &origin, sizeof(origin));
    engine.memWrite(dirAddr, &direction, sizeof(direction));

    uint8_t resultBuf[0x50] = {};
    engine.memWrite(resultAddr, resultBuf, sizeof(resultBuf));

    Win64Call call{ engine, stackTop, sentinel };
    auto records = engine.traceMemoryAccesses([&] {
        call.call(funcAddr, queryAddr, originAddr, dirAddr, resultAddr);
    });

    std::cout << "  " << records.size() << " memory accesses recorded\n";
    return classifyAndReport(records, halo1Base, halo1End, scratchBase, scratchEnd);
}
