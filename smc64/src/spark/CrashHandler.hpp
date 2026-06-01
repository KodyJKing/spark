#pragma once

namespace Spark::CrashHandler {
    // Install a first-chance VEH at highest priority.
    // On any unhandled exception: prints PID + RIP, suspends all threads, and spins
    // forever so the process stays alive for a debugger to attach.
    void install();
    void uninstall();
}
