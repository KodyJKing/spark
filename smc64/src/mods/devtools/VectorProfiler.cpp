/*
 * An ESP util for finding vectors in memory. Polls a given threadId for pointers to vectors. Writes them to a buffer if they are in range of the Camera.
 */

#include "spark/overlay/ESP.hpp"
#include "math/Vectors.hpp"
#include "memory/Memory.hpp"
#include <array>
#include <thread>
#include <unordered_set>
#include <Windows.h>

// #define ENABLE_VECTOR_PROFILER 1

struct Entry {
    Vec3 vector;
    uintptr_t pointer;
    uintptr_t rip;
    size_t offset;
    bool initialized = false;

    Vec3 getPosition() {
        auto currentPosition = Memory::safeRead<Vec3>(pointer);
        if (currentPosition.has_value()) {
            return currentPosition.value();
        }
        return vector;
    }
};

struct VectorProfiler {
    size_t writeHead = 0;
    std::array<Entry, 10000> vectors;
    
    float maxDistance = 2.0f;
    float blockSize = 0x1000;

    bool running = false;
    bool paused = true;
    std::thread profilerThread;

    void addVector(uintptr_t pointer, uintptr_t rip, size_t offset = 0) {
        if (writeHead >= vectors.size()) {
            writeHead = 0;
            return;
        }

        vectors[writeHead].vector = *(Vec3*)(pointer);
        vectors[writeHead].pointer = pointer;
        vectors[writeHead].rip = rip;
        vectors[writeHead].offset = offset;
        vectors[writeHead].initialized = true;
        writeHead++;
    }

    void maybeAddVector(uintptr_t pointer, uintptr_t rip, size_t offset = 0) {
        if (pointer < 0x10000 || pointer > 0x00007FFFFFFFFFFF)
            return;
        
        auto maybeValue = Memory::safeRead<Vec3>(pointer);
        if (!maybeValue.has_value()) return;
        auto value = maybeValue.value();

        auto diff = value - Spark::Overlay::ESP::camera.pos;
        if (diff.lengthSquared() > maxDistance * maxDistance) return;
        auto diffNormalized = diff.normalize();
        auto maxDot = cosf(Spark::Overlay::ESP::camera.fov / 2.0f);
        if (diffNormalized.dot(Spark::Overlay::ESP::camera.fwd) < maxDot) return;

        addVector(pointer, rip, offset);
    }

    void scanBlock(uintptr_t pointer, uintptr_t rip) {
        for (uintptr_t offset = 0; offset < blockSize; offset += sizeof(uintptr_t)) {
            maybeAddVector(pointer + offset * 4, rip, offset);
        }
    }

    void handleContext(CONTEXT* context) {
        uintptr_t rip = context->Rip;
        scanBlock(context->Rax, rip);
        scanBlock(context->Rbx, rip);
        scanBlock(context->Rcx, rip);
        scanBlock(context->Rdx, rip);
        scanBlock(context->Rsi, rip);
        scanBlock(context->Rdi, rip);
        scanBlock(context->R8, rip);
        scanBlock(context->R9, rip);
        scanBlock(context->R10, rip);
        scanBlock(context->R11, rip);
        scanBlock(context->R12, rip);
        scanBlock(context->R13, rip);
        scanBlock(context->R14, rip);
        scanBlock(context->R15, rip);
    }

    // Start a monitor thread to watch a given thread's context for vectors.
    void startThread(DWORD threadId) {
        // Skip if the thread is already being profiled.
        if (running)
            return;

        running = true;
        profilerThread = std::thread([this, threadId]() {
            HANDLE threadHandle = OpenThread(THREAD_ALL_ACCESS, FALSE, threadId);
            if (!threadHandle) return;

            CONTEXT context;
            context.ContextFlags = CONTEXT_ALL;

            while (running) {
                if (paused) {
                    Sleep(100);
                    continue;
                }
                
                // Suspend the thread
                if (SuspendThread(threadHandle) == -1) {
                    break;
                }

                if (GetThreadContext(threadHandle, &context)) {
                    handleContext(&context);
                }

                // Resume the thread
                if (ResumeThread(threadHandle) == -1) {
                    break;
                }

                Sleep(1);
            }

            CloseHandle(threadHandle);
        });
    }

    void stopThread() {
        if (!running)
            return;

        running = false;
        if (profilerThread.joinable())
            profilerThread.join();
    }

    void renderInfoWindow(Entry* entry) {
        ImGui::Begin("Vector Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        auto renderCopyableAddress = [](uintptr_t address, const char* label) {
            ImGui::Text("%s: 0x%p", label, (void*) address);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right-click to copy %s", label);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "0x%p", (void*) address);
                ImGui::SetClipboardText(buffer);
            }
        };

        renderCopyableAddress(entry->pointer - entry->offset, "Base Pointer");
        renderCopyableAddress(entry->pointer, "Pointer");
        renderCopyableAddress(entry->rip, "RIP");

        ImGui::Text("Offset: 0x%zX", entry->offset);
        ImGui::Text("Value: (%.2f, %.2f, %.2f)", entry->vector.x, entry->vector.y, entry->vector.z);

        ImGui::End();
    }

    void deduplicate() {
        std::unordered_set<uintptr_t> seenPointers;
        for (auto& entry : vectors) {
            if (!entry.initialized) continue;
            if (seenPointers.find(entry.pointer) != seenPointers.end()) {
                entry.initialized = false; // Mark duplicates as uninitialized
            } else {
                seenPointers.insert(entry.pointer);
            }
        }
    }
    
    void updatePositions() {
        if (paused) return;
        for (auto& entry : vectors) {
            if (!entry.initialized) continue;
            auto maybeValue = Memory::safeRead<Vec3>(entry.pointer);
            if (maybeValue.has_value()) {
                entry.vector = maybeValue.value();
            }
        }
    }

    void render() {
        // Toggle pause with numpad 9
        if (GetAsyncKeyState(VK_NUMPAD9) & 1) {
            paused = !paused;
        }

        deduplicate();
        updatePositions();

        // Select closest to look direction.
        Entry* selected = nullptr;
        float bestDot = -1.0f;
        for (size_t i = 0; i < vectors.size(); i++) {
            if (!vectors[i].initialized) continue;

            auto pos = vectors[i].vector;
            auto diff = pos - Spark::Overlay::ESP::camera.pos;
            auto diffNormalized = diff.normalize();
            auto dot = diffNormalized.dot(Spark::Overlay::ESP::camera.fwd);
            if (dot > bestDot) {
                bestDot = dot;
                selected = &vectors[i];
            }
        }
        
        for (auto& entry : vectors) {
            if (!entry.initialized) continue;
            ImU32 color = paused ? 0xFF0000FF : 0xFFFF0000;

            Vec3 pos = entry.vector;

            if (selected && entry.pointer == selected->pointer) {
                color = 0xFF00FFFF;
            }

            Spark::Overlay::ESP::drawPoint(pos, color);
        }

        if (selected) {
            renderInfoWindow(selected);
        }
    }

};

VectorProfiler profiler;

namespace Mod::DevTools::VectorProfiler {
    void start(DWORD threadId) {
        #ifdef ENABLE_VECTOR_PROFILER
        profiler.startThread(threadId);
        #endif
    }

    void stop() {
        #ifdef ENABLE_VECTOR_PROFILER
        profiler.stopThread();
        #endif
    }

    void render() {
        #ifdef ENABLE_VECTOR_PROFILER
        profiler.render();
        #endif
    }
}
