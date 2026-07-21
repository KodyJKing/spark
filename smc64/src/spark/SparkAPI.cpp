#define SPARK_EXPORTS
#include "spark/SparkAPI.h"
#include "spark/mod/ModRegistry.hpp"
#include "imgui.h"

namespace Spark {
    extern ModRegistry registry;
}

extern "C" {

void spark_registerMod(Spark::IMod* mod) {
    Spark::registry.add(mod);
}

void spark_unregisterMod(Spark::IMod* mod) {
    Spark::registry.unload(mod);
}

ImGuiContext* spark_getImGuiContext() {
    return ImGui::GetCurrentContext();
}

void spark_getImGuiAllocatorFunctions(ImGuiMemAllocFunc* outAlloc, ImGuiMemFreeFunc* outFree, void** outUserData) {
    ImGui::GetAllocatorFunctions(outAlloc, outFree, outUserData);
}

} // extern "C"
