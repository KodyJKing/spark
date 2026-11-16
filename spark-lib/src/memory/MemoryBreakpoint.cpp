#include "MemoryBreakpoint.hpp"
#include <Zydis.h>
#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace Memory {
    namespace {
        constexpr DWORD trapFlag = 0x100;

        struct Access {
            uintptr_t address;
            size_t size;
        };

        struct Breakpoint {
            uintptr_t address;
            size_t size;
            std::function<void(ThreadState&)> callback;
        };

        struct Page {
            DWORD protection;
            size_t references = 0;
        };

        struct Registry {
            std::mutex mutex;
            std::map<uintptr_t, Page> pages;
            std::vector<std::shared_ptr<Breakpoint>> breakpoints;
            PVOID handler = nullptr;
            size_t pendingSteps = 0;
            size_t pageSize;

            Registry() {
                SYSTEM_INFO info{};
                GetSystemInfo(&info);
                pageSize = info.dwPageSize;
            }
        };

        Registry& registry() {
            // VEH dispatch and pending single steps must outlive static destruction.
            static auto* value = new Registry;
            return *value;
        }

        struct PendingStep {
            std::vector<uintptr_t> pages;
            bool originalTrap = false;
            bool inCallback = false;
        };

        thread_local PendingStep pending;

        [[noreturn]] void win32Error(const char* operation) {
            throw std::system_error(GetLastError(), std::system_category(), operation);
        }

        void protect(uintptr_t page, size_t size, DWORD protection) {
            DWORD previous;
            if (!VirtualProtect(reinterpret_cast<void*>(page), size, protection, &previous))
                win32Error("Memory breakpoint VirtualProtect");
        }

        void restoreNoThrow(uintptr_t page, size_t size, DWORD protection) {
            DWORD previous;
            if (!VirtualProtect(reinterpret_cast<void*>(page), size, protection, &previous)) {
                OutputDebugStringA("Memory breakpoint: protection rollback failed\n");
                std::terminate();
            }
        }

        void uninstallIfIdle(Registry& state) {
            if (state.handler && state.breakpoints.empty() && state.pendingSteps == 0) {
                if (!RemoveVectoredExceptionHandler(state.handler))
                    win32Error("Memory breakpoint RemoveVectoredExceptionHandler");
                state.handler = nullptr;
            }
        }

        void validateRange(void* address, size_t size) {
            if (!address || !size ||
                size > (std::numeric_limits<uintptr_t>::max)() - reinterpret_cast<uintptr_t>(address))
                throw std::invalid_argument("Memory breakpoint requires a nonempty, nonoverflowing range");
        }

        bool overlaps(uintptr_t a, size_t aSize, uintptr_t b, size_t bSize) {
            return a <= b ? b - a < aSize : a - b < bSize;
        }

        uint64_t registerValue(const CONTEXT& context, ZydisRegister reg) {
            switch (ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64, reg)) {
            case ZYDIS_REGISTER_RAX: return context.Rax;
            case ZYDIS_REGISTER_RCX: return context.Rcx;
            case ZYDIS_REGISTER_RDX: return context.Rdx;
            case ZYDIS_REGISTER_RBX: return context.Rbx;
            case ZYDIS_REGISTER_RSP: return context.Rsp;
            case ZYDIS_REGISTER_RBP: return context.Rbp;
            case ZYDIS_REGISTER_RSI: return context.Rsi;
            case ZYDIS_REGISTER_RDI: return context.Rdi;
            case ZYDIS_REGISTER_R8: return context.R8;
            case ZYDIS_REGISTER_R9: return context.R9;
            case ZYDIS_REGISTER_R10: return context.R10;
            case ZYDIS_REGISTER_R11: return context.R11;
            case ZYDIS_REGISTER_R12: return context.R12;
            case ZYDIS_REGISTER_R13: return context.R13;
            case ZYDIS_REGISTER_R14: return context.R14;
            case ZYDIS_REGISTER_R15: return context.R15;
            case ZYDIS_REGISTER_RIP: return context.Rip;
            default: throw std::runtime_error("Memory breakpoint: unsupported address register");
            }
        }

        Access decodeAccess(const CONTEXT& context, const uintptr_t* fault = nullptr) {
            unsigned char code[ZYDIS_MAX_INSTRUCTION_LENGTH];
            SIZE_T count = 0;
            // A partial read can still contain the full instruction at a region boundary.
            ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(context.Rip),
                code, sizeof(code), &count);
            ZydisDecoder decoder;
            ZydisDecodedInstruction instruction;
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
            if (!count ||
                !ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)) ||
                !ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, code, count, &instruction, operands)))
                throw std::runtime_error("Memory breakpoint: cannot decode faulting instruction");

            for (size_t i = 0; i < instruction.operand_count; ++i) {
                const auto& operand = operands[i];
                if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY || !operand.actions || !operand.size)
                    continue;
                ZydisRegisterContext registers{};
                for (auto reg : { operand.mem.base, operand.mem.index }) {
                    if (reg != ZYDIS_REGISTER_NONE)
                        registers.values[reg] = registerValue(context, reg);
                }
                ZyanU64 address;
                if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddressEx(&instruction, &operand,
                    context.Rip, &registers, &address)))
                    continue;
                if (operand.mem.segment == ZYDIS_REGISTER_GS)
                    address += reinterpret_cast<uintptr_t>(NtCurrentTeb());
                if (operand.mem.segment == ZYDIS_REGISTER_FS)
                    throw std::runtime_error("Memory breakpoint: FS-relative addressing is unsupported");
                const size_t size = (operand.size + 7) / 8;
                if (!fault || (*fault >= address && *fault - address < size))
                    return { static_cast<uintptr_t>(address), size };
            }
            throw std::runtime_error("Memory breakpoint: cannot identify memory operand");
        }
    }

    struct MemoryBreakpointHandler {
        static LONG CALLBACK handle(EXCEPTION_POINTERS* exception) noexcept {
            const auto code = exception->ExceptionRecord->ExceptionCode;
            if (code != EXCEPTION_GUARD_PAGE && code != EXCEPTION_SINGLE_STEP)
                return EXCEPTION_CONTINUE_SEARCH;
            try {
                auto& state = registry();
                if (code == EXCEPTION_SINGLE_STEP) {
                    // Windows does not always populate Dr6 for trap-flag exceptions.
                    if (pending.pages.empty() || pending.inCallback)
                        return EXCEPTION_CONTINUE_SEARCH;
                    std::lock_guard lock(state.mutex);
                    for (auto address : pending.pages) {
                        auto page = state.pages.find(address);
                        if (page != state.pages.end())
                            protect(address, state.pageSize, page->second.protection | PAGE_GUARD);
                    }
                    pending.pages.clear();
                    --state.pendingSteps;
                    uninstallIfIdle(state);
                    if (!pending.originalTrap)
                        exception->ContextRecord->EFlags &= ~trapFlag;
                    // Hardware breakpoints and a debugger's trap flag belong to other handlers.
                    return pending.originalTrap || (exception->ContextRecord->Dr6 & 0xF)
                        ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_CONTINUE_EXECUTION;
                }

                if (exception->ExceptionRecord->NumberParameters < 2)
                    return EXCEPTION_CONTINUE_SEARCH;
                const auto fault = exception->ExceptionRecord->ExceptionInformation[1];
                const auto pageAddress = fault - fault % state.pageSize;
                std::vector<std::shared_ptr<Breakpoint>> callbacks;
                {
                    std::lock_guard lock(state.mutex);
                    if (!state.pages.contains(pageAddress))
                        return EXCEPTION_CONTINUE_SEARCH;
                    if (pending.pages.empty()) {
                        pending.originalTrap = (exception->ContextRecord->EFlags & trapFlag) != 0;
                        ++state.pendingSteps;
                    }
                    if (std::find(pending.pages.begin(), pending.pages.end(), pageAddress) == pending.pages.end())
                        pending.pages.push_back(pageAddress);
                    if (!pending.inCallback)
                        callbacks = state.breakpoints;
                }
                // Nested callback accesses are rearmed with the original trapped instruction,
                // not by single-stepping instructions inside the callback.
                if (pending.inCallback)
                    return EXCEPTION_CONTINUE_EXECUTION;
                exception->ContextRecord->EFlags |= trapFlag;
                if (callbacks.empty())
                    return EXCEPTION_CONTINUE_EXECUTION;

                ThreadState thread;
                thread.context = *exception->ContextRecord;
                thread.thread = GetCurrentThread();
                Access access{ fault, 1 };
                if (exception->ExceptionRecord->ExceptionInformation[0] != 8) {
                    try {
                        access = decodeAccess(thread.context, &fault);
                    } catch (const std::runtime_error& error) {
                        // Unusual instructions still support matching the OS-reported fault address.
                        OutputDebugStringA(error.what());
                        OutputDebugStringA("; operand extent unknown, matching only the fault address\n");
                        access.size = 0;
                    }
                }
                thread.m_operandAddress = reinterpret_cast<char*>(access.address);
                thread.m_operandSize = access.size;
                pending.inCallback = true;
                for (const auto& breakpoint : callbacks) {
                    bool active;
                    {
                        std::lock_guard lock(state.mutex);
                        active = std::find(state.breakpoints.begin(), state.breakpoints.end(), breakpoint)
                            != state.breakpoints.end();
                    }
                    if (active && overlaps(access.address, access.size ? access.size : 1,
                        breakpoint->address, breakpoint->size))
                        breakpoint->callback(thread);
                }
                pending.inCallback = false;
                *exception->ContextRecord = thread.context;
                exception->ContextRecord->EFlags |= trapFlag;
                return EXCEPTION_CONTINUE_EXECUTION;
            } catch (const std::exception& error) {
                OutputDebugStringA("Memory breakpoint VEH failed: ");
                OutputDebugStringA(error.what());
                std::terminate();
            } catch (...) {
                OutputDebugStringA("Memory breakpoint VEH failed with a nonstandard exception\n");
                std::terminate();
            }
        }
    };

    char* ThreadState::operandAddress() {
        return m_operandAddress ? m_operandAddress : reinterpret_cast<char*>(decodeAccess(context).address);
    }

    size_t ThreadState::operandSize() {
        return m_operandAddress ? m_operandSize : decodeAccess(context).size;
    }

    void setMemoryBreakpoint(void* address, size_t size, std::function<void(ThreadState&)> callback) {
        validateRange(address, size);
        if (!callback)
            throw std::invalid_argument("Memory breakpoint requires a callback");
        auto breakpoint = std::make_shared<Breakpoint>(Breakpoint{
            reinterpret_cast<uintptr_t>(address), size, std::move(callback) });
        auto& state = registry();
        std::lock_guard lock(state.mutex);
        for (auto& existing : state.breakpoints) {
            if (existing->address == breakpoint->address && existing->size == size) {
                existing = std::move(breakpoint);
                return;
            }
        }

        const auto first = breakpoint->address - breakpoint->address % state.pageSize;
        const auto lastByte = breakpoint->address + size - 1;
        const auto last = lastByte - lastByte % state.pageSize;
        std::vector<std::pair<uintptr_t, DWORD>> added;
        for (auto page = first;; page += state.pageSize) {
            if (!state.pages.contains(page)) {
                MEMORY_BASIC_INFORMATION info{};
                if (!VirtualQuery(reinterpret_cast<void*>(page), &info, sizeof(info)))
                    win32Error("Memory breakpoint VirtualQuery");
                const auto base = info.Protect & 0xFF;
                if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (base != PAGE_READONLY && base != PAGE_READWRITE && base != PAGE_WRITECOPY &&
                     base != PAGE_EXECUTE && base != PAGE_EXECUTE_READ &&
                     base != PAGE_EXECUTE_READWRITE && base != PAGE_EXECUTE_WRITECOPY))
                    throw std::invalid_argument("Memory breakpoint requires accessible, unguarded committed pages");
                added.emplace_back(page, info.Protect);
            }
            if (page == last)
                break;
        }

        state.breakpoints.reserve(state.breakpoints.size() + 1);
        size_t guarded = 0;
        try {
            for (const auto& [page, protection] : added)
                state.pages.emplace(page, Page{ protection });
            if (!state.handler) {
                state.handler = AddVectoredExceptionHandler(1, MemoryBreakpointHandler::handle);
                if (!state.handler)
                    win32Error("Memory breakpoint AddVectoredExceptionHandler");
            }
            for (const auto& [page, protection] : added) {
                protect(page, state.pageSize, protection | PAGE_GUARD);
                ++guarded;
            }
        } catch (...) {
            for (size_t i = 0; i < guarded; ++i)
                restoreNoThrow(added[i].first, state.pageSize, added[i].second);
            for (const auto& [page, protection] : added)
                state.pages.erase(page);
            uninstallIfIdle(state);
            throw;
        }
        for (auto page = first;; page += state.pageSize) {
            ++state.pages.at(page).references;
            if (page == last)
                break;
        }
        state.breakpoints.push_back(std::move(breakpoint));
    }

    void removeMemoryBreakpoint(void* address, size_t size) {
        validateRange(address, size);
        auto& state = registry();
        std::lock_guard lock(state.mutex);
        const auto start = reinterpret_cast<uintptr_t>(address);
        const auto breakpoint = std::find_if(state.breakpoints.begin(), state.breakpoints.end(),
            [=](const auto& value) { return value->address == start && value->size == size; });
        if (breakpoint == state.breakpoints.end())
            return;
        const auto first = start - start % state.pageSize;
        const auto lastByte = start + size - 1;
        const auto last = lastByte - lastByte % state.pageSize;
        std::vector<uintptr_t> restored;
        restored.reserve((last - first) / state.pageSize + 1);
        try {
            for (auto page = first;; page += state.pageSize) {
                if (state.pages.at(page).references == 1) {
                    protect(page, state.pageSize, state.pages.at(page).protection);
                    restored.push_back(page);
                }
                if (page == last)
                    break;
            }
        } catch (...) {
            for (auto page : restored)
                restoreNoThrow(page, state.pageSize, state.pages.at(page).protection | PAGE_GUARD);
            throw;
        }
        for (auto page = first;; page += state.pageSize) {
            if (--state.pages.at(page).references == 0)
                state.pages.erase(page);
            if (page == last)
                break;
        }
        state.breakpoints.erase(breakpoint);
        uninstallIfIdle(state);
    }
}
