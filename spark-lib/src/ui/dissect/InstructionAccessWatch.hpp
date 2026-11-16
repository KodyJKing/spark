#pragma once

#include "engine/map/schema/span_map.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SparkLib::UI::DissectTag {
    // Excludes inspection reads without excluding real game accesses on the same thread.
    class ScopedAccessCaptureSuppression {
    public:
        ScopedAccessCaptureSuppression();
        ~ScopedAccessCaptureSuppression();
        ScopedAccessCaptureSuppression(const ScopedAccessCaptureSuppression&) = delete;
        ScopedAccessCaptureSuppression& operator=(const ScopedAccessCaptureSuppression&) = delete;
    };

    struct InstructionAccess {
        uintptr_t instruction = 0;
        uint64_t hits = 0;
        uintptr_t lastAddress = 0;
        size_t lastSize = 0;
        std::string disassembly;
    };

    // UI-thread owner; the exception callback only writes to a bounded, nonblocking queue.
    // Destroy before the watched map memory is freed.
    // Construction throws std::logic_error in Spool; no breakpoint is installed there.
    class InstructionAccessWatch {
    public:
        InstructionAccessWatch(void* address, size_t size);
        ~InstructionAccessWatch();
        InstructionAccessWatch(const InstructionAccessWatch&) = delete;
        InstructionAccessWatch& operator=(const InstructionAccessWatch&) = delete;

        void stop();
        void poll();
        void* address() const { return m_address; }
        size_t size() const { return m_size; }
        uint64_t droppedHits() const;
        const std::map<uintptr_t, InstructionAccess>& instructions() const { return m_instructions; }

    private:
        struct Hit;
        struct Capture;
        void* m_address;
        size_t m_size;
        bool m_watching = false;
        std::shared_ptr<Capture> m_capture;
        std::vector<Hit> m_batch;
        std::map<uintptr_t, InstructionAccess> m_instructions;
    };

    // Returns the deepest, then smallest, intersecting parse span for the last access.
    const Engine::Map::Analysis::Span* findInstructionAccessSpan(
        const InstructionAccess& access, const std::vector<Engine::Map::Analysis::SpanInfo>& spans);
}
