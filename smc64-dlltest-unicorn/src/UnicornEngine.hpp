#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <unicorn/unicorn.h>

// RAII wrapper around a uc_engine* configured for 64-bit x86 (matches
// halo1.dll, which is a Win64 process). Thin convenience layer over the raw
// Unicorn C API -- prefer calling uc_* functions directly (via handle()) for
// anything this doesn't already cover, rather than growing this class
// unboundedly.
class UnicornEngine {
public:
    UnicornEngine() {
        check(uc_open(UC_ARCH_X86, UC_MODE_64, &m_uc), "uc_open");
    }

    ~UnicornEngine() {
        if (m_uc) uc_close(m_uc);
    }

    UnicornEngine(const UnicornEngine&) = delete;
    UnicornEngine& operator=(const UnicornEngine&) = delete;

    uc_engine* handle() const { return m_uc; }

    // Maps a region (rwx per `perms`, a UC_PROT_* combination) and writes
    // `size` bytes into it. Unicorn requires mapped regions to be 4KB-page
    // aligned in both address and size, so `address`/`size` are rounded out
    // to page boundaries automatically (matches DumpLoader's handling of
    // captured regions).
    void mapAndWrite(uint64_t address, const void* bytes, size_t size, uint32_t perms) {
        constexpr uint64_t kPageSize = 0x1000;
        uint64_t alignedBase = address & ~(kPageSize - 1);
        uint64_t alignedEnd = (address + size + kPageSize - 1) & ~(kPageSize - 1);
        check(uc_mem_map(m_uc, alignedBase, alignedEnd - alignedBase, perms), "uc_mem_map");
        check(uc_mem_write(m_uc, address, bytes, size), "uc_mem_write");
    }

    // Non-throwing variant of uc_mem_map -- useful for probing candidate
    // scratch addresses (e.g. picking a synthetic stack location that
    // doesn't collide with a loaded dump's real memory ranges) without
    // needing to catch an exception for the expected-to-sometimes-fail case.
    bool tryMap(uint64_t address, uint64_t size, uint32_t perms) {
        return uc_mem_map(m_uc, address, size, perms) == UC_ERR_OK;
    }

    void memWrite(uint64_t address, const void* bytes, size_t size) {
        check(uc_mem_write(m_uc, address, bytes, size), "uc_mem_write");
    }

    void memRead(uint64_t address, void* bytes, size_t size) {
        check(uc_mem_read(m_uc, address, bytes, size), "uc_mem_read");
    }

    // Returns Unicorn's currently mapped memory regions (address ranges are
    // inclusive on both ends, per uc_mem_region's convention). Useful for
    // demand-paging logic that must avoid re-mapping bytes that are already
    // mapped (uc_mem_map fails with UC_ERR_MAP on any overlap).
    std::vector<uc_mem_region> mappedRegions() const {
        uc_mem_region* regions = nullptr;
        uint32_t count = 0;
        check(uc_mem_regions(m_uc, &regions, &count), "uc_mem_regions");
        std::vector<uc_mem_region> result(regions, regions + count);
        if (regions) uc_free(regions);
        return result;
    }

    template <typename T>
    void regWrite(int regId, T value) {
        check(uc_reg_write(m_uc, regId, &value), "uc_reg_write");
    }

    template <typename T>
    T regRead(int regId) {
        T value{};
        check(uc_reg_read(m_uc, regId, &value), "uc_reg_read");
        return value;
    }

    // Emulates from `begin` until `until` is reached (or `timeoutMicros`
    // elapses, or `maxInstructions` is hit -- 0 means unbounded for both).
    void start(uint64_t begin, uint64_t until, uint64_t timeoutMicros = 0, size_t maxInstructions = 0) {
        check(uc_emu_start(m_uc, begin, until, timeoutMicros, maxInstructions), "uc_emu_start");
    }

    static void check(uc_err err, const char* what) {
        if (err != UC_ERR_OK) {
            throw std::runtime_error(std::string(what) + " failed: " + uc_strerror(err));
        }
    }

private:
    uc_engine* m_uc = nullptr;
};
