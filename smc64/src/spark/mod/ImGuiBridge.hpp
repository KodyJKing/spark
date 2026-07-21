#pragma once
#include "imgui.h"
#include "spark/SparkAPI.h"

// Dear ImGui's GImGui/GImAllocatorFunctions globals are private per-DLL statics
// (imconfig.h leaves IMGUI_API empty), so a mod DLL compiling its own imgui.cpp gets
// its own disconnected context and allocator. Call syncImGuiContext() before making any
// ImGui:: calls from mod code (e.g. at the top of every render-bus handler) so the mod's
// local ImGui:: symbols operate against Spark's real context/allocator instead of a null
// or mismatched one.
namespace Spark::Mod {
    inline void syncImGuiContext() {
        ImGui::SetCurrentContext(spark_getImGuiContext());

        ImGuiMemAllocFunc allocFn;
        ImGuiMemFreeFunc freeFn;
        void* userData;
        spark_getImGuiAllocatorFunctions(&allocFn, &freeFn, &userData);
        ImGui::SetAllocatorFunctions(allocFn, freeFn, userData);
    }
}
