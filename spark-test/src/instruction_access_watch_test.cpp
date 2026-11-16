#include <catch_amalgamated.hpp>
#include "ui/dissect/InstructionAccessWatch.hpp"
#include "ui/dissect/InstructionAccessWindow.hpp"
#include "ui/dissect/State.hpp"
#include <imgui_internal.h>
#include <stdexcept>
#include <thread>

using namespace SparkLib::UI::DissectTag;

namespace {
    struct WatchedPage {
        char* data;
        size_t size;

        WatchedPage() {
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            size = info.dwPageSize;
            data = static_cast<char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            if (!data)
                throw std::runtime_error("VirtualAlloc failed");
        }

        ~WatchedPage() { VirtualFree(data, 0, MEM_RELEASE); }

        DWORD protection() const {
            MEMORY_BASIC_INFORMATION info{};
            if (!VirtualQuery(data, &info, sizeof(info)))
                throw std::runtime_error("VirtualQuery failed");
            return info.Protect;
        }
    };

    __declspec(noinline) void storeByte(char* address, char value) {
        *static_cast<volatile char*>(address) = value;
    }

    __declspec(noinline) char loadByte(char* address) {
        return *static_cast<volatile char*>(address);
    }

    uint64_t hitCount(const InstructionAccessWatch& watch) {
        uint64_t total = 0;
        for (const auto& [address, instruction] : watch.instructions())
            total += instruction.hits;
        return total;
    }
}

TEST_CASE("Tag instruction watches aggregate instruction addresses with disassembly", "[instruction_access_watch]") {
    WatchedPage page;
    InstructionAccessWatch watch(page.data, 16);
    storeByte(page.data, 1);
    storeByte(page.data + 1, 2);
    REQUIRE(loadByte(page.data) == 1);
    watch.poll();
    REQUIRE(watch.instructions().size() == 2);
    REQUIRE(hitCount(watch) == 3);
    REQUIRE(watch.droppedHits() == 0);
    bool foundWrite = false;
    for (const auto& [address, instruction] : watch.instructions()) {
        REQUIRE(address != 0);
        REQUIRE(instruction.instruction == address);
        REQUIRE(instruction.disassembly.find("mov") != std::string::npos);
        REQUIRE(instruction.lastSize == 1);
        if (instruction.hits == 2) {
            foundWrite = true;
            REQUIRE(instruction.lastAddress == reinterpret_cast<uintptr_t>(page.data + 1));
        }
    }
    REQUIRE(foundWrite);
    watch.stop();
    watch.stop();
    REQUIRE(page.protection() == PAGE_READWRITE);
    storeByte(page.data, 3);
    watch.poll();
    REQUIRE(hitCount(watch) == 3);
}

TEST_CASE("Tag instruction watches exclude scoped UI reads but capture the same thread afterward", "[instruction_access_watch]") {
    WatchedPage page;
    InstructionAccessWatch watch(page.data, 1);
    {
        ScopedAccessCaptureSuppression suppress;
        storeByte(page.data, 1);
        {
            ScopedAccessCaptureSuppression nested;
            REQUIRE(loadByte(page.data) == 1);
        }
        storeByte(page.data, 2);
    }
    watch.poll();
    REQUIRE(watch.instructions().empty());
    storeByte(page.data, 3);
    watch.poll();
    REQUIRE(hitCount(watch) == 1);
    std::thread worker([&] { storeByte(page.data, 4); });
    worker.join();
    watch.poll();
    REQUIRE(hitCount(watch) == 2);
}

TEST_CASE("Tag instruction watch capture reports bounded queue overflow", "[instruction_access_watch]") {
    WatchedPage page;
    InstructionAccessWatch watch(page.data, 1);
    for (size_t i = 0; i < 4097; ++i)
        storeByte(page.data, 1);
    watch.poll();
    REQUIRE(hitCount(watch) == 4096);
    REQUIRE(watch.droppedHits() == 1);
    storeByte(page.data, 2);
    watch.poll();
    REQUIRE(hitCount(watch) == 4097);
    REQUIRE(watch.droppedHits() == 1);
}

TEST_CASE("Closing a dissect window stops its instruction watch before releasing the map", "[instruction_access_watch]") {
    WatchedPage page;
    Engine::Map::RawMapFile map;
    State windows;
    auto key = windowKey(&map, 123);
    auto [it, inserted] = windows.windows.emplace(key, WindowState{true, 123, &map});
    REQUIRE(inserted);
    it->second.instructionWatch = std::make_unique<InstructionAccessWatch>(page.data, 16);
    REQUIRE((page.protection() & PAGE_GUARD) != 0);
    it->second.isOpen = false;
    windows.cleanupWindows();
    REQUIRE(windows.windows.empty());
    REQUIRE(page.protection() == PAGE_READWRITE);
    {
        InstructionAccessWatch watch(page.data, 16);
        REQUIRE((page.protection() & PAGE_GUARD) != 0);
    }
    REQUIRE(page.protection() == PAGE_READWRITE);
}

TEST_CASE("Instruction parse span lookup chooses the innermost matching extent", "[instruction_access_watch]") {
    using namespace Engine::Map;
    char data[128]{};
    StructureNode root, child;
    std::vector<Analysis::SpanInfo> spans{
        {{data, data + 128, &root, Type::Structure}},
        {{data + 16, data + 48, &child, Type::BlockPointer, {{data, &root}}}},
        {{data + 32, data + 36, nullptr, Type::DataPointer, {{data, &root}}}}
    };
    InstructionAccess access;
    access.lastAddress = reinterpret_cast<uintptr_t>(data + 33);
    access.lastSize = 1;
    REQUIRE(findInstructionAccessSpan(access, spans) == &spans[2].span);
    access.lastAddress = reinterpret_cast<uintptr_t>(data + 31);
    access.lastSize = 2;
    REQUIRE(findInstructionAccessSpan(access, spans) == &spans[2].span);
    access.lastSize = 0;
    REQUIRE(findInstructionAccessSpan(access, spans) == &spans[1].span);
    access.lastAddress = reinterpret_cast<uintptr_t>(data + 128);
    REQUIRE(findInstructionAccessSpan(access, spans) == nullptr);
    REQUIRE(findInstructionAccessSpan(access, {}) == nullptr);
}

TEST_CASE("Instruction results update unpaused without a dissect render and close when interactive", "[instruction_access_watch]") {
    struct ImGuiOwner {
        ImGuiContext* context = ImGui::CreateContext();
        ~ImGuiOwner() { ImGui::DestroyContext(context); }
    } ui;
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1200, 900);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    struct EmptyMap : Engine::Map::MapFile {
        void* getPointerBase(Engine::Map::PointerBase) override { return nullptr; }
        Engine::Map::TagDataHeader* getTagDataHeader() override { return nullptr; }
    } map;
    WatchedPage page;
    State windows;
    auto& window = windows.windows.emplace(windowKey(&map, 0), WindowState{true, 0, &map}).first->second;
    window.instructionWatch = std::make_unique<InstructionAccessWatch>(page.data, 16);
    storeByte(page.data, 1);
    bool interactive = false;
    auto frame = [&] {
        ScopedAccessCaptureSuppression suppress;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::LogToBuffer();
        renderInstructionAccessWindows(windows, interactive);
        std::string text = ui.context->LogBuffer.c_str();
        ImGui::LogFinish();
        ImGui::Render();
        return text;
    };
    frame();
    const auto text = frame();
    REQUIRE(text.find("Instruction address") != std::string::npos);
    REQUIRE(text.find("Disassembly") != std::string::npos);
    REQUIRE(text.find("Hits") != std::string::npos);
    REQUIRE(text.find("Parse span") != std::string::npos);
    REQUIRE(text.find("mov") != std::string::npos);
    ImGuiWindow* results = nullptr;
    for (auto* candidate : ui.context->Windows)
        if (!(candidate->Flags & ImGuiWindowFlags_ChildWindow) && strstr(candidate->Name, "###TagAccess:"))
            results = candidate;
    REQUIRE(results != nullptr);
    REQUIRE((results->Flags & ImGuiWindowFlags_NoInputs) == ImGuiWindowFlags_NoInputs);
    storeByte(page.data, 2);
    frame();
    REQUIRE(hitCount(*window.instructionWatch) == 2);
    const ImVec2 close(results->Pos.x + results->Size.x - ImGui::GetStyle().FramePadding.x - ImGui::GetFontSize() * 0.5f,
        results->Pos.y + results->TitleBarHeight * 0.5f);
    io.AddMousePosEvent(close.x, close.y);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    REQUIRE(window.instructionWatch != nullptr);
    interactive = true;
    frame();
    REQUIRE((results->Flags & ImGuiWindowFlags_NoInputs) == 0);
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    REQUIRE(window.instructionWatch == nullptr);
    REQUIRE(page.protection() == PAGE_READWRITE);
}
