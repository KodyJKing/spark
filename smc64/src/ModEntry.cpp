// Entry point spark.dll's ModLoader calls (via GetProcAddress) after LoadLibrary-ing
// this DLL. See spark/src/spark/mod/ModLoader.hpp for the loader side.
#include "spark/SparkAPI.h"
#include "mods/mario/MarioMod.hpp"

extern "C" __declspec(dllexport) void spark_modLoad() {
    spark_registerMod(new MarioMod());
}
