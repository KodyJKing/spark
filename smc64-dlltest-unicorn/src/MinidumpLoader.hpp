#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <windows.h>

#include "UnicornEngine.hpp"

// Minimal parser for Windows minidump (.dmp) files -- specifically "full
// memory" dumps like the ones Task Manager's "Create dump file" produces
// (MiniDumpWriteDump w/ MiniDumpWithFullMemory). Hand-rolls the on-disk
// structures instead of pulling in <dbghelp.h>: this is a stable, documented
// disk format and we only need read-only field access, so there's no need
// for a dbghelp.dll dependency.
//
// On-disk minidump structures are packed to 4-byte alignment (the real
// Microsoft headers wrap them in pshpack4.h/poppack.h) so dumps stay
// portable between 32-bit and 64-bit tools.
#pragma pack(push, 4)
namespace MinidumpFormat {

using RVA = uint32_t;
using RVA64 = uint64_t;

struct LocationDescriptor {
    uint32_t DataSize;
    RVA Rva;
};

struct Header {
    uint32_t Signature;  // 'PMDM' (0x504d444d)
    uint32_t Version;
    uint32_t NumberOfStreams;
    RVA StreamDirectoryRva;
    uint32_t CheckSum;
    uint32_t TimeDateStamp;
    uint64_t Flags;
};

struct Directory {
    uint32_t StreamType;
    LocationDescriptor Location;
};

struct MemoryDescriptor64 {
    uint64_t StartOfMemoryRange;
    uint64_t DataSize;
};

struct Memory64List {
    uint64_t NumberOfMemoryRanges;
    RVA64 BaseRva;
    // MemoryDescriptor64 MemoryRanges[NumberOfMemoryRanges] follows
};

struct VsFixedFileInfo {
    uint32_t fields[13];
};

struct Module {
    uint64_t BaseOfImage;
    uint32_t SizeOfImage;
    uint32_t CheckSum;
    uint32_t TimeDateStamp;
    RVA ModuleNameRva;
    VsFixedFileInfo VersionInfo;
    LocationDescriptor CvRecord;
    LocationDescriptor MiscRecord;
    uint64_t Reserved0;
    uint64_t Reserved1;
};

struct ModuleList {
    uint32_t NumberOfModules;
    // Module Modules[NumberOfModules] follows
};

struct MdString {
    uint32_t Length;  // bytes, excluding null terminator
    // WCHAR Buffer[] follows
};

constexpr uint32_t kSignature = 0x504d444d;
constexpr uint32_t kStreamTypeModuleList = 4;
constexpr uint32_t kStreamTypeMemory64List = 9;

}  // namespace MinidumpFormat
#pragma pack(pop)

// Loads a Windows minidump (with full memory) into a fresh UnicornEngine
// and exposes captured module base addresses, so RVA-based function offsets
// (e.g. from ghidra_offset.py) can be resolved to absolute addresses that
// match *this specific capture's* ASLR base.
class MinidumpLoader {
public:
    explicit MinidumpLoader(const std::filesystem::path& dumpPath) {
        m_file = CreateFileW(dumpPath.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_file == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("cannot open dump: " + dumpPath.string());
        }

        m_mapping = CreateFileMappingW(m_file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!m_mapping) {
            CloseHandle(m_file);
            throw std::runtime_error("CreateFileMapping failed for: " + dumpPath.string());
        }

        m_view = static_cast<const uint8_t*>(MapViewOfFile(m_mapping, FILE_MAP_READ, 0, 0, 0));
        if (!m_view) {
            CloseHandle(m_mapping);
            CloseHandle(m_file);
            throw std::runtime_error("MapViewOfFile failed for: " + dumpPath.string());
        }

        parseDirectory();
    }

    ~MinidumpLoader() {
        if (m_view) UnmapViewOfFile(m_view);
        if (m_mapping) CloseHandle(m_mapping);
        if (m_file != INVALID_HANDLE_VALUE) CloseHandle(m_file);
    }

    MinidumpLoader(const MinidumpLoader&) = delete;
    MinidumpLoader& operator=(const MinidumpLoader&) = delete;

    // Returns the load base of `moduleName` (matched by filename, case
    // insensitive -- e.g. "halo1.dll") as captured in this dump. Throws if
    // the module isn't present.
    uint64_t moduleBase(const std::string& moduleName) const {
        return findModule(moduleName).base;
    }

    // Returns the captured SizeOfImage of `moduleName`. Throws if the
    // module isn't present. Used alongside moduleBase() to classify
    // whether a memory access falls inside the module's own image (see
    // tests/TraceMemoryAccessesTest.cpp).
    uint64_t moduleSize(const std::string& moduleName) const {
        return findModule(moduleName).size;
    }

    // Maps EVERY captured memory range into `engine`. Only usable for small
    // dumps: QEMU's softmmu backend (which Unicorn embeds) caps the total
    // number of distinct memory-region "sections" at TARGET_PAGE_SIZE
    // (4096) -- a real full-process dump (e.g. a 5GB Task Manager capture)
    // has far more individual VM regions than that and will trip a hard
    // `assert(map->sections_nb < TARGET_PAGE_SIZE)` inside
    // vendor/unicorn/qemu/exec.c. Prefer loadModule()/loadRange() to scope
    // to just the memory the function(s) under test actually touch.
    void load(UnicornEngine& engine) const {
        std::vector<const MemoryRange*> all;
        all.reserve(m_memoryRanges.size());
        for (auto& r : m_memoryRanges) all.push_back(&r);
        mapRanges(engine, all);
    }

    // Maps only the captured memory overlapping [rangeStart, rangeEnd) --
    // e.g. a single module's image. Adjacent/overlapping source ranges are
    // coalesced into as few uc_mem_map calls as possible to stay well under
    // QEMU's section-count limit (see load() above).
    void loadRange(UnicornEngine& engine, uint64_t rangeStart, uint64_t rangeEnd) const {
        std::vector<const MemoryRange*> filtered;
        for (auto& r : m_memoryRanges) {
            if (r.address + r.size <= rangeStart || r.address >= rangeEnd) continue;
            filtered.push_back(&r);
        }
        mapRanges(engine, filtered);
    }

    // Convenience wrapper: loads just `moduleName`'s own image range as
    // captured in this dump (its BaseOfImage/SizeOfImage). Throws if the
    // module isn't present.
    void loadModule(UnicornEngine& engine, const std::string& moduleName) const {
        const ModuleInfo& mod = findModule(moduleName);
        loadRange(engine, mod.base, mod.base + mod.size);
    }

    // Installs a UC_HOOK_MEM_UNMAPPED hook on `engine` so that whenever
    // emulated code touches an address that isn't mapped yet, this dump is
    // consulted for a captured range covering that address; if found, just
    // that range is paged in (mapped + written) on demand and emulation
    // retries the faulting access. This lets globals-dependent functions
    // (pointers into heap-resident pools, etc.) work without having to
    // manually chase and pre-load every referenced region ahead of time,
    // while still never mapping more of the dump than a given call path
    // actually touches (staying well under Unicorn/QEMU's ~4096 memory
    // section limit -- see load() above).
    //
    // `engine` and this MinidumpLoader must both outlive any emulation that
    // relies on this hook. Only one demand-paging hook per loader/engine
    // pair is expected; calling this more than once for the same engine
    // will register additional redundant hooks.
    void installDemandPaging(UnicornEngine& engine) const {
        m_hookEngine = &engine;
        uc_hook hook;
        UnicornEngine::check(
            uc_hook_add(engine.handle(), &hook, UC_HOOK_MEM_UNMAPPED,
                reinterpret_cast<void*>(&MinidumpLoader::onUnmappedAccess),
                const_cast<MinidumpLoader*>(this), uint64_t{ 1 }, uint64_t{ 0 }),
            "uc_hook_add(UC_HOOK_MEM_UNMAPPED)");
    }

    // Ensures the captured dump range(s) overlapping [address, address+size)
    // are mapped into `engine`, mapping+writing whatever isn't already
    // present. Unlike installDemandPaging()'s hook (which only fires for
    // *emulated CPU* memory accesses during uc_emu_start), this is for
    // direct host-side engine.memRead()/memWrite() calls -- e.g. walking a
    // linked data structure without executing any real code -- since
    // uc_mem_read/uc_mem_write bypass the CPU hook path entirely and won't
    // trigger it. Returns false if no captured range covers `address` at
    // all (e.g. reading from truly unallocated memory).
    bool ensureMapped(UnicornEngine& engine, uint64_t address, uint64_t size) const {
        return pageInCovering(engine, address, size);
    }

private:
    struct MemoryRange {
        uint64_t address;
        uint64_t size;
        const uint8_t* data;  // points directly into the mapped .dmp view
    };

    struct ModuleInfo {
        uint64_t base;
        uint64_t size;
    };

    const ModuleInfo& findModule(const std::string& moduleName) const {
        auto it = m_modules.find(toLower(moduleName));
        if (it == m_modules.end()) {
            throw std::runtime_error("module not found in dump: " + moduleName);
        }
        return it->second;
    }

    // uc_cb_eventmem_t trampoline -- `userData` is the MinidumpLoader
    // instance passed to installDemandPaging(). Returning true tells
    // Unicorn the memory is now available and to retry the faulting
    // access; false lets it fail normally (genuinely not in the dump).
    static bool onUnmappedAccess(uc_engine* /*uc*/, uc_mem_type /*type*/, uint64_t address,
                                  int size, int64_t /*value*/, void* userData) {
        auto* self = static_cast<const MinidumpLoader*>(userData);
        if (!self->m_hookEngine) return false;
        return self->pageInCovering(*self->m_hookEngine, address, size > 0 ? static_cast<uint64_t>(size) : 1);
    }

    // Finds every captured range overlapping [address, address+size), and
    // maps+writes whatever portion of each isn't already mapped in
    // `engine`. Tracked page-granular against this loader's own
    // m_pagedIntervals (not by re-querying Unicorn's uc_mem_regions() each
    // call, which is comparatively very expensive and would make
    // byte-at-a-time callers, like reading a C string, extremely slow).
    // Returns false if no captured range covers `address` at all.
    bool pageInCovering(UnicornEngine& engine, uint64_t address, uint64_t size) const {
        uint64_t rangeStart = address;
        uint64_t rangeEnd = address + size;

        std::vector<const MemoryRange*> hits;
        for (auto& r : m_memoryRanges) {
            if (r.address + r.size <= rangeStart || r.address >= rangeEnd) continue;
            hits.push_back(&r);
        }
        if (hits.empty()) return false;

        constexpr uint64_t kPageSize = 0x1000;
        for (auto* r : hits) {
            uint64_t blockStart = r->address & ~(kPageSize - 1);
            uint64_t blockEnd = (r->address + r->size + kPageSize - 1) & ~(kPageSize - 1);

            uint64_t page = blockStart;
            while (page < blockEnd) {
                if (isPageTracked(page)) {
                    page += kPageSize;
                    continue;
                }
                uint64_t runEnd = page + kPageSize;
                while (runEnd < blockEnd && !isPageTracked(runEnd)) runEnd += kPageSize;

                UnicornEngine::check(
                    uc_mem_map(engine.handle(), page, runEnd - page, UC_PROT_ALL),
                    "uc_mem_map (demand page)");
                trackInterval(page, runEnd);

                page = runEnd;
            }

            if (r->size > 0) {
                engine.memWrite(r->address, r->data, r->size);
            }
        }

        return true;
    }

    // Returns true if `page` (must be page-aligned) falls inside an
    // interval this loader has already mapped (see trackInterval()).
    bool isPageTracked(uint64_t page) const {
        for (auto& iv : m_pagedIntervals) {
            if (page < iv.first) break;  // sorted ascending, non-overlapping -- no later interval can match either
            if (page < iv.second) return true;
        }
        return false;
    }

    // Records [start, end) as now-mapped, merging with any adjacent or
    // overlapping intervals already tracked so the list stays small and
    // sorted.
    void trackInterval(uint64_t start, uint64_t end) const {
        m_pagedIntervals.push_back({ start, end });
        std::sort(m_pagedIntervals.begin(), m_pagedIntervals.end());
        std::vector<std::pair<uint64_t, uint64_t>> merged;
        for (auto& iv : m_pagedIntervals) {
            if (!merged.empty() && iv.first <= merged.back().second) {
                merged.back().second = (std::max)(merged.back().second, iv.second);
            } else {
                merged.push_back(iv);
            }
        }
        m_pagedIntervals = std::move(merged);
    }

    // Maps `ranges` (need not be pre-sorted) into `engine`, merging any
    // ranges that are adjacent or overlapping once page-aligned into a
    // single uc_mem_map call, to minimize the number of distinct memory
    // sections registered with Unicorn/QEMU. Tracks every mapped block in
    // m_pagedIntervals so later ensureMapped()/demand-paging calls know
    // this memory is already present.
    void mapRanges(UnicornEngine& engine, std::vector<const MemoryRange*> ranges) const {
        constexpr uint64_t kPageSize = 0x1000;
        std::sort(ranges.begin(), ranges.end(),
            [](const MemoryRange* a, const MemoryRange* b) { return a->address < b->address; });

        size_t i = 0;
        while (i < ranges.size()) {
            uint64_t blockStart = ranges[i]->address & ~(kPageSize - 1);
            uint64_t blockEnd = (ranges[i]->address + ranges[i]->size + kPageSize - 1) & ~(kPageSize - 1);

            size_t j = i + 1;
            while (j < ranges.size()) {
                uint64_t nextStart = ranges[j]->address & ~(kPageSize - 1);
                if (nextStart > blockEnd) break;  // gap -- ends this block
                uint64_t nextEnd = (ranges[j]->address + ranges[j]->size + kPageSize - 1) & ~(kPageSize - 1);
                blockEnd = (std::max)(blockEnd, nextEnd);
                ++j;
            }

            // Mapped RWX regardless of the dump's real per-page protections
            // -- this harness has no need to emulate protection faults
            // faithfully, it just needs the actual bytes available to
            // read/execute.
            UnicornEngine::check(
                uc_mem_map(engine.handle(), blockStart, blockEnd - blockStart, UC_PROT_ALL),
                "uc_mem_map");
            trackInterval(blockStart, blockEnd);

            for (size_t k = i; k < j; ++k) {
                if (ranges[k]->size > 0) {
                    engine.memWrite(ranges[k]->address, ranges[k]->data, ranges[k]->size);
                }
            }

            i = j;
        }
    }

    void parseDirectory() {
        auto* header = reinterpret_cast<const MinidumpFormat::Header*>(m_view);
        if (header->Signature != MinidumpFormat::kSignature) {
            throw std::runtime_error("not a minidump file (bad signature)");
        }

        auto* dirs = reinterpret_cast<const MinidumpFormat::Directory*>(
            m_view + header->StreamDirectoryRva);

        for (uint32_t i = 0; i < header->NumberOfStreams; ++i) {
            const auto& dir = dirs[i];
            if (dir.StreamType == MinidumpFormat::kStreamTypeMemory64List) {
                parseMemory64List(dir.Location.Rva);
            } else if (dir.StreamType == MinidumpFormat::kStreamTypeModuleList) {
                parseModuleList(dir.Location.Rva);
            }
        }

        if (m_memoryRanges.empty()) {
            throw std::runtime_error(
                "dump has no Memory64List stream -- was it captured as a *full* dump "
                "(Task Manager 'Create dump file'), not a mini/heap dump?");
        }
    }

    void parseMemory64List(uint32_t rva) {
        auto* list = reinterpret_cast<const MinidumpFormat::Memory64List*>(m_view + rva);
        auto* descriptors = reinterpret_cast<const MinidumpFormat::MemoryDescriptor64*>(
            m_view + rva + sizeof(MinidumpFormat::Memory64List));

        uint64_t cursor = list->BaseRva;
        m_memoryRanges.reserve(static_cast<size_t>(list->NumberOfMemoryRanges));
        for (uint64_t i = 0; i < list->NumberOfMemoryRanges; ++i) {
            const auto& desc = descriptors[i];
            m_memoryRanges.push_back({ desc.StartOfMemoryRange, desc.DataSize, m_view + cursor });
            cursor += desc.DataSize;
        }
    }

    void parseModuleList(uint32_t rva) {
        auto* list = reinterpret_cast<const MinidumpFormat::ModuleList*>(m_view + rva);
        auto* modules = reinterpret_cast<const MinidumpFormat::Module*>(
            m_view + rva + sizeof(MinidumpFormat::ModuleList));

        for (uint32_t i = 0; i < list->NumberOfModules; ++i) {
            const auto& mod = modules[i];
            auto* str = reinterpret_cast<const MinidumpFormat::MdString*>(m_view + mod.ModuleNameRva);
            auto* wchars = reinterpret_cast<const wchar_t*>(
                reinterpret_cast<const uint8_t*>(str) + sizeof(MinidumpFormat::MdString));
            std::wstring wname(wchars, str->Length / sizeof(wchar_t));

            // Module paths are ASCII in practice (Windows system/game
            // paths) -- narrow-truncate rather than pulling in a full
            // UTF-8 conversion for this.
            std::string name(wname.begin(), wname.end());

            auto slash = name.find_last_of("\\/");
            std::string fileName = (slash == std::string::npos) ? name : name.substr(slash + 1);
            m_modules[toLower(fileName)] = ModuleInfo{ mod.BaseOfImage, mod.SizeOfImage };
        }
    }

    static std::string toLower(std::string s) {
        for (auto& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    HANDLE m_file = INVALID_HANDLE_VALUE;
    HANDLE m_mapping = nullptr;
    const uint8_t* m_view = nullptr;

    std::vector<MemoryRange> m_memoryRanges;
    std::unordered_map<std::string, ModuleInfo> m_modules;
    mutable UnicornEngine* m_hookEngine = nullptr;
    // Merged, sorted, page-aligned [start,end) intervals this loader has
    // already mapped into *some* engine -- tracked internally rather than
    // via uc_mem_regions() so repeated ensureMapped() checks (e.g. reading
    // a C string byte-by-byte) don't pay for a round-trip into Unicorn each
    // time.
    mutable std::vector<std::pair<uint64_t, uint64_t>> m_pagedIntervals;
};
