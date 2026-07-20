#pragma once

#include <cstdint>
#include <stdexcept>

#include "UnicornEngine.hpp"
#include "MinidumpLoader.hpp"

// Shared (MinidumpLoader, UnicornEngine) pair for tests whose full call
// chain has been confirmed -- via Ghidra decompile, not assumed -- to
// never write to anything but its own caller-owned scratch/output
// buffers, i.e. it never mutates global/heap-resident game state (entity
// pool, tag array, caches, etc.). Sharing one engine across such tests
// avoids repeatedly re-mapping halo1.dll's whole image and re-paging-in
// the same globals that most tests in this suite end up touching anyway
// (entity pool, tag array, BSP data).
//
// IMPORTANT: MinidumpLoader's demand-paging/range tracking
// (m_pagedIntervals) is tracked per-*loader*, not per-*engine* -- pairing
// this fixture's loader with any UnicornEngine other than the one owned
// here would make it think that other engine already has pages mapped
// that it actually doesn't (silently skipping the real mapping). Never
// call dump()/engine() and mix-and-match with a different engine/loader.
//
// Do NOT opt a test into this fixture unless you've actually decompiled
// its full call chain and confirmed it. Mark opted-in tests with a
// one-line comment recording that, e.g.:
//
//   // SHARED-SAFE: confirmed via Ghidra (2026-07-19) -- only writes to
//   // param_4 (caller output buffer), never to global/pool memory.
//
// If in doubt, keep using a private MinidumpLoader + UnicornEngine for
// that test instead (see SmokeTest.cpp for the from-scratch pattern, or
// any pre-2026-07-20 test for the old per-test dump+engine pattern).
//
// Possible future improvement: this contract is currently trust-based,
// not enforced -- a debug-only mode that snapshots (or hooks writes to)
// touched pages before/after a "shared-safe" test and flags any mutation
// outside its own scratch slot would catch a mistaken opt-in automatically.
// Not implemented; Unicorn has no built-in memory-snapshot/diff primitive,
// so this would need to be hand-rolled (e.g. via UC_HOOK_MEM_WRITE).
class SharedDumpFixture {
public:
    static constexpr const char* kDumpPath =
        R"(C:\code\projects\hacking\smc64\smc64-dlltest-unicorn\dumps\MCC-Win64-Shipping.DMP)";

    // Scratch slot size/layout matches the convention already used by
    // Win64Call-based tests: stackTop = slot+0x10000, sentinel =
    // slot+0x11000, argsBase = slot+0x12000.
    static constexpr uint64_t kSlotSize = 0x20000;

    static SharedDumpFixture& instance() {
        static SharedDumpFixture fixture;
        return fixture;
    }

    MinidumpLoader& dump() { return m_dump; }
    UnicornEngine& engine() { return m_engine; }

    // Hands out the next kSlotSize-byte scratch slot from the shared pool
    // -- a private stack + args region for one test. Slots are never
    // freed/reused: tests run serially (not concurrently) and there are
    // few enough of them that kPoolSize comfortably covers the whole
    // suite; bump-allocating avoids each test having to redo its own
    // tryMap-candidate-probing loop against a pool that's already shared.
    uint64_t allocateScratchSlot() {
        if (m_nextSlot + kSlotSize > m_scratchBase + kPoolSize) {
            throw std::runtime_error(
                "SharedDumpFixture: scratch pool exhausted -- increase kPoolSize");
        }
        uint64_t slot = m_nextSlot;
        m_nextSlot += kSlotSize;
        return slot;
    }

    SharedDumpFixture(const SharedDumpFixture&) = delete;
    SharedDumpFixture& operator=(const SharedDumpFixture&) = delete;

private:
    static constexpr uint64_t kPoolSize = 0x1000000;  // 16MB -- 128 slots

    SharedDumpFixture() : m_dump(kDumpPath) {
        m_dump.loadModule(m_engine, "halo1.dll");
        m_dump.installDemandPaging(m_engine);

        for (uint64_t candidate : { 0x0000700000000000ULL, 0x0000600000000000ULL,
                                     0x0000500000000000ULL, 0x0000200000000000ULL }) {
            if (m_engine.tryMap(candidate, kPoolSize, UC_PROT_ALL)) {
                m_scratchBase = candidate;
                m_nextSlot = candidate;
                return;
            }
        }
        throw std::runtime_error(
            "SharedDumpFixture: could not find a free scratch region in the dumped address space");
    }

    MinidumpLoader m_dump;
    UnicornEngine m_engine;
    uint64_t m_scratchBase = 0;
    uint64_t m_nextSlot = 0;
};
