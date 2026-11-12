#include <catch_amalgamated.hpp>
#include "memory/MemoryBreakpoint.hpp"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
    struct TestPages {
        char* data;
        size_t pageSize;
        std::vector<std::pair<void*, size_t>> watches;

        TestPages() {
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            pageSize = info.dwPageSize;
            data = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2,
                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            if (!data)
                throw std::runtime_error("VirtualAlloc failed");
        }

        ~TestPages() {
            for (const auto& [address, size] : watches)
                Memory::removeMemoryBreakpoint(address, size);
            VirtualFree(data, 0, MEM_RELEASE);
        }

        void watch(size_t offset, size_t size, std::function<void(Memory::ThreadState&)> callback) {
            watches.emplace_back(data + offset, size);
            Memory::setMemoryBreakpoint(data + offset, size, std::move(callback));
        }

        DWORD protection(size_t offset = 0) {
            MEMORY_BASIC_INFORMATION info{};
            if (!VirtualQuery(data + offset, &info, sizeof(info)))
                throw std::runtime_error("VirtualQuery failed");
            return info.Protect;
        }
    };

    __declspec(noinline) void writeByte(char* address, char value) {
        *static_cast<volatile char*>(address) = value;
    }

    __declspec(noinline) char readByte(char* address) {
        return *static_cast<volatile char*>(address);
    }

    __declspec(noinline) void writeWord(char* address, uint32_t value) {
        *reinterpret_cast<volatile uint32_t*>(address) = value;
    }

    bool raiseAndCatch(DWORD code) {
        __try {
            RaiseException(code, 0, 0, nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return true;
        }
        return false;
    }

    bool readAndCatch(char* address) {
        __try {
            readByte(address);
        } __except (GetExceptionCode() == EXCEPTION_GUARD_PAGE
            ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            return true;
        }
        return false;
    }
}

TEST_CASE("Memory breakpoints rearm and report the trapped operand", "[memory_breakpoint]") {
    TestPages pages;
    int hits = 0;
    char* operand = nullptr;
    size_t size = 0;
    DWORD threadId = 0;
    bool beforeWrite = false;
    pages.watch(16, 1, [&](Memory::ThreadState& thread) {
        ++hits;
        operand = thread.operandAddress();
        size = thread.operandSize();
        threadId = GetThreadId(thread.thread);
        beforeWrite = pages.data[16] == 0;
    });
    REQUIRE((pages.protection() & PAGE_GUARD) != 0);
    writeByte(pages.data + 16, 7);
    REQUIRE(hits == 1);
    REQUIRE(beforeWrite);
    REQUIRE(operand == pages.data + 16);
    REQUIRE(size == 1);
    REQUIRE(threadId == GetCurrentThreadId());
    REQUIRE((pages.protection() & PAGE_GUARD) != 0);
    REQUIRE(readByte(pages.data + 16) == 7);
    REQUIRE(hits == 2);
    writeByte(pages.data + 17, 8);
    REQUIRE(hits == 2);
    writeByte(pages.data + 16, 9);
    REQUIRE(hits == 3);
    Memory::removeMemoryBreakpoint(pages.data + 16, 1);
    REQUIRE(pages.protection() == PAGE_READWRITE);
    writeByte(pages.data + 16, 10);
    REQUIRE(hits == 3);
}

TEST_CASE("Memory breakpoints share pages, replace exact watches, and span pages", "[memory_breakpoint]") {
    TestPages pages;
    int first = 0, second = 0, replacement = 0;
    pages.watch(8, 8, [&](auto&) { ++first; });
    pages.watch(10, 2, [&](auto&) { ++second; });
    writeByte(pages.data + 10, 1);
    REQUIRE(first == 1);
    REQUIRE(second == 1);
    pages.watch(8, 8, [&](auto&) { ++replacement; });
    Memory::removeMemoryBreakpoint(pages.data + 10, 2);
    REQUIRE((pages.protection() & PAGE_GUARD) != 0);
    writeByte(pages.data + 10, 2);
    REQUIRE(first == 1);
    REQUIRE(second == 1);
    REQUIRE(replacement == 1);

    int spanning = 0;
    pages.watch(pages.pageSize - 1, 2, [&](auto&) { ++spanning; });
    writeByte(pages.data + pages.pageSize - 1, 1);
    writeByte(pages.data + pages.pageSize, 1);
    REQUIRE(spanning == 2);
    Memory::removeMemoryBreakpoint(pages.data + pages.pageSize - 1, 2);
    REQUIRE(pages.protection(pages.pageSize) == PAGE_READWRITE);
    REQUIRE((pages.protection() & PAGE_GUARD) != 0);
}

TEST_CASE("Memory breakpoint matching includes operands beginning outside a watch", "[memory_breakpoint]") {
    TestPages pages;
    int hits = 0;
    size_t size = 0;
    char* operand = nullptr;
    pages.watch(17, 1, [&](Memory::ThreadState& thread) {
        ++hits;
        operand = thread.operandAddress();
        size = thread.operandSize();
    });
    writeWord(pages.data + 16, 123);
    REQUIRE(hits == 1);
    REQUIRE(operand == pages.data + 16);
    REQUIRE(size == sizeof(uint32_t));
}

TEST_CASE("Memory breakpoint callbacks can remove themselves and run on worker threads", "[memory_breakpoint]") {
    TestPages pages;
    int hits = 0;
    DWORD callbackThread = 0;
    pages.watch(0, 1, [&](Memory::ThreadState& thread) {
        ++hits;
        callbackThread = GetThreadId(thread.thread);
        Memory::removeMemoryBreakpoint(pages.data, 1);
    });
    DWORD workerId = 0;
    std::thread worker([&] {
        workerId = GetCurrentThreadId();
        writeByte(pages.data, 1);
        writeByte(pages.data, 2);
    });
    worker.join();
    REQUIRE(hits == 1);
    REQUIRE(callbackThread == workerId);
    REQUIRE(pages.protection() == PAGE_READWRITE);
}

TEST_CASE("Memory breakpoints preserve unrelated exceptions even during callbacks", "[memory_breakpoint]") {
    TestPages pages;
    bool singleStepCaught = false, foreignGuardCaught = false;
    DWORD old;
    REQUIRE(VirtualProtect(pages.data + pages.pageSize, pages.pageSize, PAGE_READWRITE | PAGE_GUARD, &old));
    pages.watch(0, 1, [&](auto&) {
        singleStepCaught = raiseAndCatch(EXCEPTION_SINGLE_STEP);
        foreignGuardCaught = readAndCatch(pages.data + pages.pageSize);
    });
    REQUIRE(raiseAndCatch(EXCEPTION_SINGLE_STEP));
    writeByte(pages.data, 1);
    REQUIRE(singleStepCaught);
    REQUIRE(foreignGuardCaught);
    REQUIRE((pages.protection() & PAGE_GUARD) != 0);
}

TEST_CASE("Memory breakpoints defer rearming recursive callback accesses", "[memory_breakpoint]") {
    TestPages pages;
    int first = 0, second = 0;
    pages.watch(0, 1, [&](auto&) {
        ++first;
        writeByte(pages.data + pages.pageSize, 42);
    });
    pages.watch(pages.pageSize, 1, [&](auto&) { ++second; });
    writeByte(pages.data, 1);
    REQUIRE(first == 1);
    REQUIRE(second == 0);
    REQUIRE((pages.protection() & PAGE_GUARD) != 0);
    REQUIRE((pages.protection(pages.pageSize) & PAGE_GUARD) != 0);
    REQUIRE(readByte(pages.data + pages.pageSize) == 42);
    REQUIRE(second == 1);
}

TEST_CASE("Removing a memory breakpoint from another callback suppresses it", "[memory_breakpoint]") {
    TestPages pages;
    int second = 0;
    pages.watch(0, 2, [&](auto&) { Memory::removeMemoryBreakpoint(pages.data + 1, 1); });
    pages.watch(1, 1, [&](auto&) { ++second; });
    writeByte(pages.data + 1, 1);
    REQUIRE(second == 0);
}

TEST_CASE("Memory breakpoint operands can cross into a guarded page", "[memory_breakpoint]") {
    TestPages pages;
    int hits = 0;
    char* address = nullptr;
    size_t size = 0;
    pages.watch(pages.pageSize, 1, [&](Memory::ThreadState& thread) {
        ++hits;
        address = thread.operandAddress();
        size = thread.operandSize();
    });
    writeWord(pages.data + pages.pageSize - 2, 123);
    REQUIRE(hits == 1);
    REQUIRE(address == pages.data + pages.pageSize - 2);
    REQUIRE(size == 4);
    REQUIRE((pages.protection(pages.pageSize) & PAGE_GUARD) != 0);
}

TEST_CASE("Memory breakpoints apply context edits and support execute guards", "[memory_breakpoint]") {
    TestPages pages;
    // mov byte ptr [rcx], dl; ret
    const unsigned char code[] = { 0x88, 0x11, 0xC3 };
    memcpy(pages.data + pages.pageSize, code, sizeof(code));
    DWORD old;
    REQUIRE(VirtualProtect(pages.data + pages.pageSize, pages.pageSize, PAGE_EXECUTE_READ, &old));
    REQUIRE(FlushInstructionCache(GetCurrentProcess(), pages.data + pages.pageSize, sizeof(code)));
    auto function = reinterpret_cast<void(*)(char*, char)>(pages.data + pages.pageSize);
    int writes = 0, executions = 0;
    pages.watch(0, 1, [&](Memory::ThreadState& thread) {
        ++writes;
        thread.context.Rdx = 42;
    });
    function(pages.data, 1);
    REQUIRE(writes == 1);
    Memory::removeMemoryBreakpoint(pages.data, 1);
    REQUIRE(readByte(pages.data) == 42);
    char* operand = nullptr;
    size_t size = 0;
    pages.watch(pages.pageSize, 1, [&](Memory::ThreadState& thread) {
        ++executions;
        operand = thread.operandAddress();
        size = thread.operandSize();
    });
    function(pages.data, 2);
    function(pages.data, 3);
    REQUIRE(executions == 2);
    REQUIRE(operand == pages.data + pages.pageSize);
    REQUIRE(size == 1);
    Memory::removeMemoryBreakpoint(pages.data + pages.pageSize, 1);
    REQUIRE(pages.protection(pages.pageSize) == PAGE_EXECUTE_READ);
}

TEST_CASE("Memory breakpoints reject invalid ranges and preserve foreign guards", "[memory_breakpoint]") {
    TestPages pages;
    auto callback = [](auto&) {};
    REQUIRE_THROWS_AS(Memory::setMemoryBreakpoint(nullptr, 1, callback), std::invalid_argument);
    REQUIRE_THROWS_AS(Memory::setMemoryBreakpoint(pages.data, 0, callback), std::invalid_argument);
    REQUIRE_THROWS_AS(Memory::setMemoryBreakpoint(pages.data, 1, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(Memory::setMemoryBreakpoint(
        reinterpret_cast<void*>(std::numeric_limits<uintptr_t>::max()), 2, callback), std::invalid_argument);
    DWORD old;
    REQUIRE(VirtualProtect(pages.data + pages.pageSize, pages.pageSize, PAGE_READWRITE | PAGE_GUARD, &old));
    REQUIRE_THROWS_AS(Memory::setMemoryBreakpoint(pages.data, pages.pageSize * 2, callback), std::invalid_argument);
    REQUIRE(pages.protection() == PAGE_READWRITE);
    REQUIRE(pages.protection(pages.pageSize) == (PAGE_READWRITE | PAGE_GUARD));
    REQUIRE(VirtualProtect(pages.data + pages.pageSize, pages.pageSize, PAGE_NOACCESS, &old));
    REQUIRE_THROWS_AS(Memory::setMemoryBreakpoint(pages.data, pages.pageSize * 2, callback), std::invalid_argument);
    REQUIRE(pages.protection() == PAGE_READWRITE);
    REQUIRE(VirtualProtect(pages.data + pages.pageSize, pages.pageSize, PAGE_READONLY, &old));
    pages.watch(pages.pageSize, 1, callback);
    REQUIRE(readByte(pages.data + pages.pageSize) == 0);
    Memory::removeMemoryBreakpoint(pages.data + pages.pageSize, 1);
    REQUIRE(pages.protection(pages.pageSize) == PAGE_READONLY);
}
