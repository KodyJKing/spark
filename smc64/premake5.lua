project "smc64"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    systemversion "latest"
    staticruntime "off" -- critical: keeps CRT/heap shared with spark.dll

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../obj/" .. outputdir .. "/%{prj.name}")

    files {
        "pch.h",
        "src/**.hpp",
        "src/**.cpp",
        -- Private copy of core ImGui (no backends — spark.dll owns the real
        -- Win32/DX11 backend and render loop). Must sync context/allocator via
        -- Spark::Mod::syncImGuiContext() before any ImGui:: call.
        "../vendor/imgui/*.h",
        "../vendor/imgui/*.cpp",
        "../vendor/libsm64/dist/include/*.h",
        "../vendor/libsm64/dist/include/decomp/*.h",
        "../vendor/libsm64/dist/include/decomp/PR/*.h",
    }

    includedirs {
        ".",
        "src",
        "../spark/src",
        "../vendor/minhook/include",
        "../vendor/imgui",
        "../vendor/libsm64/dist/include",
        "../vendor/libsm64/dist/include/decomp",
    }

    defines {
        "VERSION_US",
        "NO_SEGMENTED_MEMORY",
    }

    forceincludes { "pch.h" }

    links {
        "spark",
        "../vendor/libsm64/dist/sm64",
    }

    filter "configurations:Debug"
        runtime "Debug"
        optimize "Off"
        editandcontinue "Off"
        flags { "NoIncrementalLink" }
        buildoptions { "/JMC-" }
        symbols "Full"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"
