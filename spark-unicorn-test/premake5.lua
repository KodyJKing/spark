project "spark-unicorn-test"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    systemversion "latest"
    staticruntime "off"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../obj/" .. outputdir .. "/%{prj.name}")

    files {
        "src/**.hpp",
        "src/**.cpp"
    }

    includedirs {
        "src",
        "../spark/src",
        "../vendor/unicorn/include",
        "../vendor/imgui"
    }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"
        libdirs { "../vendor/unicorn/build/Debug" }

    filter "configurations:Release"
        runtime "Release"
        optimize "On"
        libdirs { "../vendor/unicorn/build/Release" }

    filter {}
        -- Built via scripts/build_unicorn.ps1 (vendor/unicorn is CMake-based,
        -- not compiled inline via premake -- see that script for why).
        -- "unicorn" (not "unicorn-static") is CMake's bundled archive
        -- combining unicorn-static + unicorn-common + x86_64-softmmu.
        links { "unicorn" }
