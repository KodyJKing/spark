#include <Windows.h>
#include <iostream>
#include <filesystem>
#include "utils/Console.hpp"
#include "utils/UnloadLock.hpp"
#include "utils/Utils.hpp"
#include "spark/Spark.hpp"
#include "spark/SparkHost.hpp"
#include "MinHook.h"
#include "spark/overlay/Overlay.hpp"
#include "spark/CrashHandler.hpp"

// MainThread
DWORD WINAPI MainThread(LPVOID _hModule) {
    HMODULE hModule = (HMODULE) _hModule;
    Console::alloc();
    Console::toggleConsole();
    Spark::CrashHandler::install();

    std::cout << "Load method: " << (Utils::isInjected() ? "Injection" : "Imported") << std::endl;
    if (!Utils::isInjected()) { // Give the game some time to load.
        // This module seems to load late in the game's initialization.
        Utils::waitForModule("PartyWin.dll");
    }

    do {
        SparkHost::bReinit = false;
        
        MH_Initialize();
        Spark::Overlay::init();
        
        while (!SparkHost::bExit && !SparkHost::bReinit) {
            if (GetAsyncKeyState(VK_F8) & 1)
                Console::toggleConsole();
            if ( Utils::isInjected() && (GetAsyncKeyState(VK_F9) & 1) ) { // Only allow uninjecting if the mod was injected.
                SparkHost::exit();
                break;
            }
            if (GetAsyncKeyState(VK_F10) & 1) {
                SparkHost::reinitialize();
                break;
            }
            Spark::modThreadUpdate();
            Sleep(1000 / 60);
        }
        
        waitForSafeUnload();
        Spark::free();
        Spark::Overlay::free();
        MH_Uninitialize();
        
    } while (SparkHost::bReinit);


    waitForSafeUnload();
    Sleep(200); // Extra time for hooks to exit.

    Spark::CrashHandler::uninstall();
    Console::free();

    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

// Dll Main
// We dll export this so MSDetours setdll.exe can add it to a carrier dll.
BOOL __declspec(dllexport) APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            CreateThread(0, 0, MainThread, hModule, 0, 0);
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}