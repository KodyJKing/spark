#include "InstructionAccessWatch.hpp"
#include "memory/MemoryBreakpoint.hpp"
#include <Zydis.h>
#include <array>
#include <atomic>
#include <mutex>

namespace SparkLib::UI::DissectTag {
    namespace {
        thread_local size_t captureSuppression = 0;
        constexpr size_t captureCapacity = 4096;
    }

    ScopedAccessCaptureSuppression::ScopedAccessCaptureSuppression() { ++captureSuppression; }
    ScopedAccessCaptureSuppression::~ScopedAccessCaptureSuppression() { --captureSuppression; }

    struct InstructionAccessWatch::Hit {
        uintptr_t instruction;
        uintptr_t address;
        size_t size;
        std::array<unsigned char, ZYDIS_MAX_INSTRUCTION_LENGTH> code{};
        SIZE_T codeSize = 0;
        DWORD readError = ERROR_SUCCESS;
    };

    struct InstructionAccessWatch::Capture {
        std::atomic<bool> enabled = true;
        std::atomic<uint64_t> dropped = 0;
        std::mutex mutex;
        std::array<Hit, captureCapacity> queue;
        size_t count = 0;

        void record(Memory::ThreadState& thread) {
            if (captureSuppression || !enabled.load(std::memory_order_relaxed))
                return;
            Hit hit{};
            hit.instruction = thread.context.Rip;
            hit.address = reinterpret_cast<uintptr_t>(thread.operandAddress());
            hit.size = thread.operandSize();
            if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(hit.instruction),
                hit.code.data(), hit.code.size(), &hit.codeSize))
                hit.readError = GetLastError();

            std::unique_lock lock(mutex, std::try_to_lock);
            if (!lock.owns_lock() || count == queue.size()) {
                dropped.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            queue[count++] = hit;
        }
    };

    InstructionAccessWatch::InstructionAccessWatch(void* address, size_t size)
        : m_address(address), m_size(size), m_capture(std::make_shared<Capture>()) {
#ifdef SPOOL
        throw std::logic_error("Tag instruction watching is not available in Spool");
#else
        ScopedAccessCaptureSuppression suppress;
        m_batch.reserve(captureCapacity);
        Memory::setMemoryBreakpoint(address, size, [weak = std::weak_ptr<Capture>(m_capture)](auto& thread) {
            if (auto capture = weak.lock())
                capture->record(thread);
        });
        m_watching = true;
#endif
    }

    InstructionAccessWatch::~InstructionAccessWatch() {
        try {
            stop();
        } catch (const std::exception& error) {
            OutputDebugStringA("Instruction access watch could not unwatch: ");
            OutputDebugStringA(error.what());
            std::terminate();
        }
    }

    void InstructionAccessWatch::stop() {
        if (!m_watching)
            return;
        ScopedAccessCaptureSuppression suppress;
        m_capture->enabled = false;
        try {
            Memory::removeMemoryBreakpoint(m_address, m_size);
        } catch (...) {
            m_capture->enabled = true;
            throw;
        }
        m_watching = false;
    }

    uint64_t InstructionAccessWatch::droppedHits() const {
        return m_capture->dropped.load(std::memory_order_relaxed);
    }

    void InstructionAccessWatch::poll() {
        ScopedAccessCaptureSuppression suppress;
        m_batch.clear();
        {
            std::lock_guard lock(m_capture->mutex);
            m_batch.insert(m_batch.end(), m_capture->queue.begin(),
                m_capture->queue.begin() + m_capture->count);
            m_capture->count = 0;
        }
        for (const auto& hit : m_batch) {
            auto [it, inserted] = m_instructions.try_emplace(hit.instruction);
            auto& instruction = it->second;
            if (inserted) {
                instruction.instruction = hit.instruction;
                ZydisDisassembledInstruction decoded;
                if (!hit.codeSize) {
                    instruction.disassembly = "[instruction read failed: Win32 " + std::to_string(hit.readError) + "]";
                } else {
                    const auto status = ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64, hit.instruction,
                        hit.code.data(), hit.codeSize, &decoded);
                    instruction.disassembly = ZYAN_SUCCESS(status) ? decoded.text
                        : "[disassembly failed: Zydis " + std::to_string(status) + "]";
                }
            }
            ++instruction.hits;
            instruction.lastAddress = hit.address;
            instruction.lastSize = hit.size;
        }
    }

    const Engine::Map::Analysis::Span* findInstructionAccessSpan(
        const InstructionAccess& access, const std::vector<Engine::Map::Analysis::SpanInfo>& spans) {
        const Engine::Map::Analysis::Span* best = nullptr;
        for (const auto& info : spans) {
            const auto start = reinterpret_cast<uintptr_t>(info.span.start);
            const auto end = reinterpret_cast<uintptr_t>(info.span.end);
            if (end <= start)
                continue;
            const auto size = access.lastSize ? access.lastSize : 1;
            const bool intersects = access.lastAddress <= start
                ? start - access.lastAddress < size : access.lastAddress - start < end - start;
            if (!intersects)
                continue;
            if (!best || info.span.ancestors.size() > best->ancestors.size() ||
                (info.span.ancestors.size() == best->ancestors.size() &&
                    end - start < reinterpret_cast<uintptr_t>(best->end) - reinterpret_cast<uintptr_t>(best->start)))
                best = &info.span;
        }
        return best;
    }
}
