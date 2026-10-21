-- spark-lib: code shared by spark (the injected mod host) and spool (the standalone app).
-- It is a source library for now: consumers call sparkLib() inside their project to compile
-- these sources directly. The "spark-lib" project below is a kind "None" project that exists
-- only so the shared files are browsable in the IDE.

local libRoot = path.getabsolute(".")

function sparkLib()
    files {
        libRoot .. "/src/**.hpp",
        libRoot .. "/src/**.h",
        libRoot .. "/src/**.cpp",
    }

    includedirs { libRoot .. "/src" }

    -- Engine and memory headers expect the contents of the precompiled header.
    forceincludes { "pch.h" }

    defines {
        "VERSION_US",
        "NO_SEGMENTED_MEMORY",
    }
end

project "spark-lib"
    kind "None"
    location "."
    files {
        "src/**.hpp",
        "src/**.h",
        "src/**.cpp",
    }
