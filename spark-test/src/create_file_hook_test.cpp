#include <catch_amalgamated.hpp>
#include "spark/tag-injection/CreateFileHook.hpp"
#include "MinHook.h"
#include <Windows.h>
#include <filesystem>

TEST_CASE("Map-open loader hooks preserve ordinary DLL loads and uninstall cleanly", "[configured_injection]") {
    REQUIRE(MH_Initialize() == MH_OK);
    struct Hooks {
        ~Hooks() {
            Spark::TagInjection::uninstallCreateFileHook();
            CHECK(MH_Uninitialize() == MH_OK);
        }
    } cleanup;
    REQUIRE_NOTHROW(Spark::TagInjection::installCreateFileHook());
    wchar_t directory[MAX_PATH];
    const auto count = GetSystemDirectoryW(directory, MAX_PATH);
    REQUIRE(count > 0);
    REQUIRE(count < MAX_PATH);
    const auto path = std::filesystem::path(directory) / "version.dll";
    for (int i = 0; i < 2; ++i) {
        auto module = LoadLibraryExW(path.c_str(), nullptr, 0);
        REQUIRE(module != nullptr);
        const auto address = GetProcAddress(module, "GetFileVersionInfoW");
        const auto released = FreeLibrary(module);
        REQUIRE(address != nullptr);
        REQUIRE(released);
        REQUIRE_NOTHROW(Spark::TagInjection::uninstallCreateFileHook());
        REQUIRE_NOTHROW(Spark::TagInjection::installCreateFileHook());
    }
}
