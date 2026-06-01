#include "CrashHandler.hpp"

#include "utils/Console.hpp"

#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <string>

namespace Spark::CrashHandler {

static void*                          s_vehHandle = nullptr;
static std::unordered_map<DWORD,bool> s_decisions; // true = stop, false = ignore
static std::mutex                     s_mutex;
static std::filesystem::path          s_decisionsPath;

// File format: one entry per line, '+HEXCODE' = stop, '-HEXCODE' = ignore, '#' = comment
static void loadDecisions() {
    std::ifstream f(s_decisionsPath);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.size() < 2 || line[0] == '#') continue;
        bool stop = (line[0] == '+');
        if (line[0] != '+' && line[0] != '-') continue;
        try {
            DWORD code = (DWORD)std::stoul(line.substr(1), nullptr, 16);
            s_decisions[code] = stop;
        } catch (...) {}
    }
}

static void saveDecisions() {
    std::ofstream f(s_decisionsPath, std::ios::trunc);
    if (!f) return;
    f << "# CrashHandler decisions — '+CODE' = stop, '-CODE' = ignore\n";
    for (auto& [code, stop] : s_decisions)
        f << (stop ? '+' : '-') << std::hex << (unsigned long)code << "\n";
}

static void suspendOtherThreads(DWORD tid, DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{ sizeof(te) };
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid && te.th32ThreadID != tid) {
                HANDLE ht = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (ht) { SuspendThread(ht); CloseHandle(ht); }
            }
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}

static LONG WINAPI handler(EXCEPTION_POINTERS* ep) {
    DWORD     code = ep->ExceptionRecord->ExceptionCode;
    DWORD     pid  = GetCurrentProcessId();
    DWORD     tid  = GetCurrentThreadId();
    ULONG_PTR rip  = ep->ContextRecord->Rip;

    std::unique_lock<std::mutex> lock(s_mutex);

    auto it = s_decisions.find(code);
    if (it == s_decisions.end()) {
        // First time seeing this code — ask the user.
        char msg[320];
        snprintf(msg, sizeof(msg),
            "Unknown exception: 0x%08X\n"
            "TID: %lu    RIP: 0x%016llX\n\n"
            "Yes  =  suspend process (attach debugger)\n"
            "No   =  ignore and pass to next handler\n\n"
            "(Your choice will be remembered.)",
            (unsigned)code, (unsigned long)tid, (unsigned long long)rip);
        int choice = MessageBoxA(nullptr, msg, "CrashHandler — Unknown Exception",
                                 MB_YESNO | MB_ICONWARNING | MB_SYSTEMMODAL | MB_SETFOREGROUND);
        bool stop = (choice == IDYES);
        s_decisions[code] = stop;
        saveDecisions();
        if (!stop) return EXCEPTION_CONTINUE_SEARCH;
    } else if (!it->second) {
        // Previously decided to ignore.
        return EXCEPTION_CONTINUE_SEARCH;
    }

    lock.unlock();

    printf("\n[CrashHandler] CAUGHT 0x%08X\n", (unsigned)code);
    printf("[CrashHandler]   PID: %lu\n",       (unsigned long)pid);
    printf("[CrashHandler]   TID: %lu  <-- offending thread\n", (unsigned long)tid);
    printf("[CrashHandler]   RIP: 0x%016llX\n", (unsigned long long)rip);
    fflush(stdout);
    std::cout << "[CrashHandler] Suspending all other threads — attach a debugger now." << std::endl;

    Console::showConsole(true);

    suspendOtherThreads(tid, pid);

    char exitMsg[320];
    snprintf(exitMsg, sizeof(exitMsg),
        "Exception 0x%08X caught.\n"
        "TID: %lu    RIP: 0x%016llX\n\n"
        "All other threads are suspended.\n"
        "Attach a debugger, then click OK to terminate the process.",
        (unsigned)code, (unsigned long)tid, (unsigned long long)rip);
    MessageBoxA(nullptr, exitMsg, "CrashHandler — Process Suspended",
                MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND);
    TerminateProcess(GetCurrentProcess(), (UINT)code);
}

void install() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    s_decisionsPath = std::filesystem::path(exePath).parent_path() / L"crash_handler_decisions.txt";

    loadDecisions();
    printf("[CrashHandler] Decisions: %s (%zu entries)\n",
           s_decisionsPath.string().c_str(), s_decisions.size());

    s_vehHandle = AddVectoredExceptionHandler(1, handler);
    std::cout << "[CrashHandler] VEH installed (" << s_vehHandle << ")" << std::endl;
}

void uninstall() {
    if (s_vehHandle) {
        RemoveVectoredExceptionHandler(s_vehHandle);
        s_vehHandle = nullptr;
        std::cout << "[CrashHandler] VEH removed" << std::endl;
    }
}

} // namespace Spark::CrashHandler
