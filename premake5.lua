workspace "spark"
   architecture "x64"
   configurations { "Debug", "Release" }
   startproject "spark"

   defines { 
      "ZYDIS_STATIC_BUILD",
      "ASMJIT_STATIC",
   }

   platforms { "Win64" }

   filter "platforms:Win64"
       system "Windows"
       architecture "x86_64"

outputdir = "%{cfg.buildcfg}-%{cfg.platform}"

group "Dependencies"
   include "vendor/minhook"
   include "vendor/zydis"
group ""

include "spark/premake5.lua"
include "spark-launcher/premake5.lua"
include "spark-dll-test/premake5.lua"
include "spark-unicorn-test/premake5.lua"