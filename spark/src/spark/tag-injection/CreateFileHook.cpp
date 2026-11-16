#include "CreateFileHook.hpp"
#include "engine/map/spark/static_tag_injection/configured_injection.hpp"
#include "utils/Utils.hpp"
#include "utils/UnloadLock.hpp"
#include "MinHook.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>

namespace Spark::TagInjection {
    namespace {
        using CreateFile = HANDLE(*)(LPCSTR, uint32_t);
        using LoadLibrary = HMODULE(WINAPI*)(LPCWSTR, HANDLE, DWORD);
        using ReleaseLibrary = BOOL(WINAPI*)(HMODULE);
        constexpr uintptr_t haloCreateFileOffset = 0xCCFB0;
        CreateFile originalCreateFile = nullptr;
        LoadLibrary originalLoadLibrary = nullptr;
        ReleaseLibrary originalFreeLibrary = nullptr;
        HMODULE hookedModule = nullptr;
        void* loadTarget = nullptr;
        void* freeTarget = nullptr;
        std::shared_ptr<Engine::Map::Spark::ConfiguredInjection> injection;
        std::recursive_mutex stateMutex;
        std::atomic<uint32_t> activeCreates = 0;
        std::condition_variable_any createsFinished;
        std::mutex notificationMutex;
        std::set<std::string> notifications;
        std::atomic<bool> installed = false;

        void report(const std::string& error) {
            std::cerr << "[TagInjection] " << error << '\n';
            {
                std::lock_guard lock(notificationMutex);
                if (!notifications.insert(error).second) return;
            }
            const auto message = error + "\n\nThe original map will be used where possible. "
                "Content may be missing and the mod may be unstable.";
            MessageBoxA(nullptr, message.c_str(), "Spark - Tag injection failed", MB_OK | MB_ICONWARNING);
        }

        void check(MH_STATUS status, const char* operation) {
            if (status != MH_OK)
                throw std::runtime_error(std::string(operation) + ": " + MH_StatusToString(status));
        }

        HANDLE detourCreateFile(LPCSTR filePath, uint32_t flags) {
            UnloadLock unload;
            struct ActiveCall {
                ActiveCall() { ++activeCreates; }
                ~ActiveCall() {
                    std::lock_guard lock(stateMutex);
                    --activeCreates;
                    createsFinished.notify_all();
                }
            } active;
            // Keep the trampoline alive until this call finishes; FreeLibrary waits in detach().
            std::unique_lock lock(stateMutex);
            const auto original = originalCreateFile;
            const auto service = injection;
            try {
                if (filePath && service) {
                    if (auto prepared = service->prepare(filePath, flags)) {
                        for (const auto& warning : prepared->warnings)
                            std::clog << "[TagInjection] " << warning << '\n';
                        const auto path = prepared->path.string();
                        auto handle = original(path.c_str(), flags);
                        if (handle) {
                            std::clog << "[TagInjection] " << filePath << " -> " << path << '\n';
                            return handle;
                        }
                        throw std::runtime_error("Cannot open injected cache " + path +
                            " (Win32 error " + std::to_string(GetLastError()) + ")");
                    }
                }
            } catch (const std::exception& error) {
                report(std::string("While opening ") + (filePath ? filePath : "<null>") + ": " + error.what());
            }
            return original(filePath, flags);
        }

        // Called after LoadLibraryExW returns, never while executing a DLL notification.
        void attach(HMODULE module) {
            std::lock_guard lock(stateMutex);
            if (!installed || !module || hookedModule == module) return;
            if (hookedModule) throw std::runtime_error("Previous halo1 file hook was not detached");
            auto target = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(module) + haloCreateFileOffset);
            constexpr std::array<uint8_t, 6> prefix{0x48, 0x83, 0xEC, 0x48, 0x8B, 0xC2};
            if (std::memcmp(target, prefix.data(), prefix.size()) != 0)
                throw std::runtime_error("haloCreateFile signature mismatch; this halo1 build is unsupported");
            check(MH_CreateHook(target, reinterpret_cast<void*>(&detourCreateFile),
                reinterpret_cast<void**>(&originalCreateFile)), "Create haloCreateFile hook");
            const auto enabled = MH_EnableHook(target);
            if (enabled != MH_OK) {
                const auto removed = MH_RemoveHook(target);
                if (removed != MH_OK)
                    std::cerr << "[TagInjection] Remove failed hook: " << MH_StatusToString(removed) << '\n';
                originalCreateFile = nullptr;
                check(enabled, "Enable haloCreateFile hook");
            }
            hookedModule = module;
        }

        void detach() {
            std::unique_lock lock(stateMutex);
            if (!hookedModule) return;
            auto target = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(hookedModule) + haloCreateFileOffset);
            check(MH_DisableHook(target), "Disable haloCreateFile hook");
            createsFinished.wait(lock, [] { return activeCreates.load() == 0; });
            check(MH_RemoveHook(target), "Remove haloCreateFile hook");
            hookedModule = nullptr;
            originalCreateFile = nullptr;
        }

        HMODULE WINAPI detourLoadLibrary(LPCWSTR path, HANDLE file, DWORD flags) {
            UnloadLock unload;
            auto module = originalLoadLibrary(path, file, flags);
            const auto error = GetLastError();
            if (module && !(flags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE |
                LOAD_LIBRARY_AS_IMAGE_RESOURCE | DONT_RESOLVE_DLL_REFERENCES))) {
                if (GetModuleHandleW(L"halo1.dll") == module) {
                    try { attach(module); }
                    catch (const std::exception& e) { report(e.what()); }
                }
            }
            SetLastError(error);
            return module;
        }

        BOOL WINAPI detourFreeLibrary(HMODULE module) {
            UnloadLock unload;
            bool halo = false;
            {
                std::lock_guard lock(stateMutex);
                halo = hookedModule && hookedModule == module;
            }
            if (halo) {
                try { detach(); }
                catch (const std::exception& e) { report(e.what()); }
            }
            auto result = originalFreeLibrary(module);
            const auto error = GetLastError();
            if (halo) {
                try { attach(GetModuleHandleW(L"halo1.dll")); }
                catch (const std::exception& e) { report(e.what()); }
            }
            SetLastError(error);
            return result;
        }
    }

    void installCreateFileHook() {
        if (installed) return;
        injection = std::make_shared<Engine::Map::Spark::ConfiguredInjection>(
            Utils::getHalo1Directory() / "maps", Utils::getModsDirectory().parent_path() / "tags",
            Utils::getTagSchemaPath());
        auto kernel = GetModuleHandleW(L"kernel32.dll");
        loadTarget = reinterpret_cast<void*>(GetProcAddress(kernel, "LoadLibraryExW"));
        freeTarget = reinterpret_cast<void*>(GetProcAddress(kernel, "FreeLibrary"));
        if (!loadTarget || !freeTarget) throw std::runtime_error("Cannot resolve library lifecycle functions");
        check(MH_CreateHook(loadTarget, reinterpret_cast<void*>(&detourLoadLibrary),
            reinterpret_cast<void**>(&originalLoadLibrary)), "Create LoadLibraryExW observer");
        try {
            check(MH_CreateHook(freeTarget, reinterpret_cast<void*>(&detourFreeLibrary),
                reinterpret_cast<void**>(&originalFreeLibrary)), "Create FreeLibrary observer");
            installed = true;
            check(MH_EnableHook(loadTarget), "Enable LoadLibraryExW observer");
            check(MH_EnableHook(freeTarget), "Enable FreeLibrary observer");
            attach(GetModuleHandleW(L"halo1.dll"));
        } catch (const std::exception&) {
            uninstallCreateFileHook();
            throw;
        }
    }

    void uninstallCreateFileHook() {
        installed = false;
        for (auto target : {loadTarget, freeTarget}) {
            if (!target) continue;
            auto status = MH_DisableHook(target);
            if (status != MH_OK && status != MH_ERROR_DISABLED && status != MH_ERROR_NOT_CREATED)
                std::cerr << "[TagInjection] Disable observer: " << MH_StatusToString(status) << '\n';
        }
        try { detach(); }
        catch (const std::exception& e) { std::cerr << "[TagInjection] " << e.what() << '\n'; }
        waitForSafeUnload();
        for (auto target : {loadTarget, freeTarget}) {
            if (!target) continue;
            auto status = MH_RemoveHook(target);
            if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
                std::cerr << "[TagInjection] Remove observer: " << MH_StatusToString(status) << '\n';
        }
        loadTarget = freeTarget = nullptr;
        injection.reset();
        notifications.clear();
    }
}
