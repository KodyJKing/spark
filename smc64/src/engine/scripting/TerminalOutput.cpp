#include "TerminalOutput.hpp"
#include "engine/common.hpp"
#include <algorithm>
#include <cstring>

namespace Engine::TerminalOutput {

    // --- Offsets (relative to halo1.dll base) ---
    // Pool pointer global:  DAT_7fff4bdccdc8 (offset 0x2D9CDC8)
    // Head index global:    DAT_7fff4bdccdd0 (offset 0x2D9CDD0), u32, 0xFFFFFFFF = empty
    static constexpr uintptr_t kPoolPtrOffset  = 0x2D9CDC8U;
    static constexpr uintptr_t kHeadIdxOffset  = 0x2D9CDD0U;

    // Per-entry layout inside the pool (stride = 0x124 bytes):
    //   +0x04  prev index (u32)  - newer entry
    //   +0x08  next index (u32)  - older entry
    //   +0x0D  char text[0xFE]
    //   +0x110 float color[4]   - RGBA
    static constexpr uint32_t kEntryStride     = 0x124U;
    static constexpr uint32_t kNextOffset      = 0x08U;
    static constexpr uint32_t kTextOffset      = 0x0DU;
    static constexpr uint32_t kTextMax         = 0xFEU;
    static constexpr uint32_t kColorOffset     = 0x110U;
    static constexpr uint32_t kNone            = 0xFFFFFFFFU;

    // The pool data section starts at pool_ptr + *(i32*)(pool_ptr + 0x34).
    // Confirmed at runtime to be 0x38, but we read it each time for safety.
    static constexpr uint32_t kPoolDataOffsetField = 0x34U;

    static uint32_t s_lastHead  = kNone;
    static bool     s_ready     = false;

    std::vector<Entry> poll() {
        const uintptr_t base = Engine::dllBase();

        const uintptr_t poolPtr = *reinterpret_cast<uintptr_t*>(base + kPoolPtrOffset);
        if (!poolPtr) return {};

        const uint32_t head = *reinterpret_cast<uint32_t*>(base + kHeadIdxOffset);

        if (!s_ready) {
            // First call: snapshot cursor so we don't replay old history.
            s_lastHead = head;
            s_ready    = true;
            return {};
        }

        if (head == s_lastHead) return {};

        const uintptr_t dataOff  = static_cast<uintptr_t>(
            *reinterpret_cast<int32_t*>(poolPtr + kPoolDataOffsetField));
        const uintptr_t dataBase = poolPtr + dataOff;

        std::vector<Entry> newEntries;
        uint32_t idx = head;
        while (idx != kNone && idx != s_lastHead) {
            const uintptr_t entryBase = dataBase + static_cast<uintptr_t>(idx) * kEntryStride;

            Entry e;
            const char* src = reinterpret_cast<const char*>(entryBase + kTextOffset);
            e.text.assign(src, ::strnlen(src, kTextMax));
            ::memcpy(e.color, reinterpret_cast<const void*>(entryBase + kColorOffset), sizeof(e.color));

            newEntries.push_back(std::move(e));
            idx = *reinterpret_cast<uint32_t*>(entryBase + kNextOffset);
        }

        // Reverse so we push oldest first.
        std::reverse(newEntries.begin(), newEntries.end());
        s_lastHead = head;
        return newEntries;
    }

} // namespace Engine::TerminalOutput
