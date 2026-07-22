project "spark"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    systemversion "latest"
    staticruntime "off"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../obj/" .. outputdir .. "/%{prj.name}")

    files {
        "pch.h",
        "pch.cpp",
        "src/**.hpp",
        "src/**.cpp",
        "../vendor/imgui/*.h", 
        "../vendor/imgui/*.cpp",
        "../vendor/imgui/backends/imgui_impl_win32.*",
        "../vendor/imgui/backends/imgui_impl_dx11.*",
    }

    includedirs {
        "src",
        "../vendor/minhook/include",
        "../vendor/imgui/backends",
        "../vendor/imgui",
        "../vendor/zydis",
    }

    defines {
        "VERSION_US",
        "NO_SEGMENTED_MEMORY",
    }

    pchheader "pch.h"
    pchsource "pch.cpp"
    forceincludes "pch.h"

    links { 
        "MinHook",
        "Zydis",
    }

    filter "system:windows"
        systemversion "latest"
        files { 'versioninfo.rc' }
        vpaths { ['Resources/*'] = { '*.rc' } }

    filter "configurations:Debug"
        runtime "Debug"
        optimize "Off"
        editandcontinue "Off"
        flags { "NoIncrementalLink" }
        buildoptions { "/JMC-" }

        --  Turn symbols off when you're using Cheat Engine as part of your workflow. 
        --  Cheat Engine will keep .pdb files open and prevent the linker from writing to them.
        symbols "Full"
        -- symbols "Off"
  
    filter "configurations:Release"
        runtime "Release"
        optimize "On"
