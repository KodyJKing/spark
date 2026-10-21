project "spool"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++20"
    systemversion "latest"
    staticruntime "off"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../obj/" .. outputdir .. "/%{prj.name}")

    -- Spool compiles spark-lib (shared engine code and tool UIs); nothing from the game-hooking half of spark.
    sparkLib()

    files {
        "src/**.hpp",
        "src/**.cpp",
        "../vendor/imgui/*.h",
        "../vendor/imgui/*.cpp",
        "../vendor/imgui/backends/imgui_impl_win32.*",
        "../vendor/imgui/backends/imgui_impl_dx11.*",
    }

    includedirs {
        "src",
        "../spark/src",
        "../vendor/imgui",
        "../vendor/imgui/backends",
        "../vendor/json",
        "../vendor/minhook/include",
        "../vendor/zydis",
    }

    defines {
        -- Memory.hpp pulls in SparkAPI.h; mark SPARK_API as exported so it isn't dllimport.
        "SPARK_EXPORTS",
        "SPOOL",
    }

    buildoptions { "/FS" }
    links { "MinHook", "Zydis", "d3d11", "dxgi", "d3dcompiler", "dwmapi", "ole32", "shell32" }

    -- The tag dissector loads its schema from next to the exe.
    postbuildcommands {
        '{COPYFILE} "%{prj.location}/../spark/shipfiles/tags.schema2.json" "%{cfg.targetdir}/tags.schema2.json"'
    }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "Full"
        editandcontinue "Off"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"
