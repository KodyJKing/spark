project "spool"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++20"
    systemversion "latest"
    staticruntime "off"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../obj/" .. outputdir .. "/%{prj.name}")

    -- Spool compiles the tool UIs and the map/schema code it shares with the injected spark mod
    -- straight from spark's sources. Nothing from the game-hooking half of spark is included.
    local sparkSrc = "../spark/src/"

    files {
        "src/**.hpp",
        "src/**.cpp",
        sparkSrc .. "engine/map/map_file.cpp",
        sparkSrc .. "engine/map/raw_map_file.cpp",
        sparkSrc .. "engine/map/tag_data_types.cpp",
        sparkSrc .. "engine/map/schema/*.cpp",
        sparkSrc .. "mods/devtools/tag_browser/TagBrowser.cpp",
        sparkSrc .. "mods/devtools/dissect/DissectTag.cpp",
        sparkSrc .. "utils/Strings.cpp",
        sparkSrc .. "utils/ImGuiUtils.cpp",
        sparkSrc .. "utils/Utils.cpp",
        sparkSrc .. "memory/Memory.cpp",
        "../vendor/imgui/*.h",
        "../vendor/imgui/*.cpp",
        "../vendor/imgui/backends/imgui_impl_win32.*",
        "../vendor/imgui/backends/imgui_impl_dx11.*",
    }

    includedirs {
        "src",
        "../spark",
        "../spark/src",
        "../vendor/imgui",
        "../vendor/imgui/backends",
        "../vendor/json",
    }

    -- The shared sources expect spark's precompiled header contents.
    forceincludes { "pch.h" }

    defines {
        "VERSION_US",
        "NO_SEGMENTED_MEMORY",
        -- Memory.hpp pulls in SparkAPI.h; mark SPARK_API as exported so it isn't dllimport.
        "SPARK_EXPORTS",
        "SPOOL",
    }

    buildoptions { "/FS" }

    links { "d3d11", "dxgi", "d3dcompiler", "dwmapi", "ole32", "shell32" }

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
