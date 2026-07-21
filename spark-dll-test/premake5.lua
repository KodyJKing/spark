project "spark-dll-test"
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
        "../spark/src"
    }

    -- /EHa (not just /EHsc) so the SEH translator in main.cpp can turn a bad
    -- "this function is pure" hypothesis (e.g. an access violation) into a
    -- catchable C++ exception instead of taking down the whole harness.
    buildoptions { "/EHa" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"
