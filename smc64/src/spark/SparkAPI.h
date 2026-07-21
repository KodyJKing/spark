#pragma once
#include "imgui.h"

#ifdef SPARK_EXPORTS
#define SPARK_API __declspec(dllexport)
#else
#define SPARK_API __declspec(dllimport)
#endif

namespace Spark { class IMod; }

extern "C" {
    // Register a mod with Spark. Spark takes ownership; destroy() is called on teardown.
    // The mod must have been allocated in the calling module (destroy() calls delete this).
    SPARK_API void spark_registerMod(Spark::IMod* mod);

    // Hot-unload a previously registered mod.
    SPARK_API void spark_unregisterMod(Spark::IMod* mod);

    // Dear ImGui's GImGui/GImAllocatorFunctions globals are private statics per DLL
    // (IMGUI_API is a no-op in imconfig.h), so a mod DLL that compiles its own imgui.cpp
    // gets its own disconnected context/allocator. These let a mod fetch Spark's real
    // instances and sync via ImGui::SetCurrentContext()/SetAllocatorFunctions() before
    // making any ImGui:: calls. See spark/mod/ImGuiBridge.hpp for the convenience wrapper.
    SPARK_API ImGuiContext* spark_getImGuiContext();
    SPARK_API void spark_getImGuiAllocatorFunctions(ImGuiMemAllocFunc* outAlloc, ImGuiMemFreeFunc* outFree, void** outUserData);
}
