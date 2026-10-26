-- spark-test: Catch2 unit tests for spark-lib.
-- Test output (e.g. dumped map files) goes to spark-test/out, which is gitignored.

local testRoot = path.getabsolute(".")

project "spark-test"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    systemversion "latest"
    staticruntime "off"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../obj/" .. outputdir .. "/%{prj.name}")

    sparkLib()

    files {
        "src/**.hpp",
        "src/**.cpp",
        "../vendor/catch2/catch_amalgamated.hpp",
        "../vendor/catch2/catch_amalgamated.cpp",
        "../vendor/imgui/*.h",
        "../vendor/imgui/*.cpp",
        "../vendor/imgui/backends/imgui_impl_win32.*",
        "../vendor/imgui/backends/imgui_impl_dx11.*",
    }

    includedirs {
        "src",
        "../spark/src",
        "../vendor/catch2",
        "../vendor/imgui",
        "../vendor/imgui/backends",
        "../vendor/json",
        "../vendor/minhook/include",
        "../vendor/zydis",
    }

    defines {
        -- Memory.hpp pulls in SparkAPI.h; mark SPARK_API as exported so it isn't dllimport.
        "SPARK_EXPORTS",
        -- The force-included pch pulls in Windows.h, whose min/max macros break Catch2.
        "NOMINMAX",
        "SPARK_TEST_DIR=R\"(" .. testRoot .. ")\"",
    }

    buildoptions { "/FS", "/bigobj" }
    links { "MinHook", "Zydis", "d3d11", "dxgi", "d3dcompiler", "dwmapi", "ole32", "shell32" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "Full"
        editandcontinue "Off"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"
