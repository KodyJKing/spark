#pragma once

namespace Spark::CrashHandler {
    // Install a first-chance VEH at highest priority.
    // On any unhandled exception: prints PID + RIP, suspends all threads, and spins
    // forever so the process stays alive for a debugger to attach.
    void install();
    void uninstall();

    void setStatusInt(const char* name, int value);
    void incrementStatusInt(const char* name, int delta);
}
