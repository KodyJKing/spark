#pragma once

#include <cstdint>

#include <unicorn/x86.h>

#include "UnicornEngine.hpp"

// Helper for invoking a function inside an emulated Win64 process using the
// standard Win64 fastcall convention (RCX, RDX, R8, R9 for the first four
// integer/pointer args; RAX for the return value).
//
// `stackTop` should be a 16-byte-aligned address near the top of a mapped,
// writable stack region with reasonable headroom below it (the callee's own
// prologue will reserve its shadow space/locals there -- we don't need to
// pre-reserve it ourselves).
//
// `returnAddress` is a sentinel: it's pushed as the return address and used
// as uc_emu_start's `until` bound, so emulation halts the instant the
// callee's `ret` would jump there. It must be a *mapped* address (Unicorn
// validates the target of `until` even though it's never actually fetched
// from in practice), but its contents don't matter.
//
// Only handles the first 4 integer/pointer args for now -- extend with
// stack args (5th+) and XMM0-3 (floating point) once a real test needs them.
struct Win64Call {
    UnicornEngine& engine;
    uint64_t stackTop;
    uint64_t returnAddress;

    uint64_t call(uint64_t target, uint64_t a0 = 0, uint64_t a1 = 0, uint64_t a2 = 0, uint64_t a3 = 0) {
        uint64_t rsp = stackTop - sizeof(uint64_t);
        engine.memWrite(rsp, &returnAddress, sizeof(returnAddress));

        engine.regWrite<uint64_t>(UC_X86_REG_RSP, rsp);
        engine.regWrite<uint64_t>(UC_X86_REG_RCX, a0);
        engine.regWrite<uint64_t>(UC_X86_REG_RDX, a1);
        engine.regWrite<uint64_t>(UC_X86_REG_R8, a2);
        engine.regWrite<uint64_t>(UC_X86_REG_R9, a3);

        engine.start(target, returnAddress);

        return engine.regRead<uint64_t>(UC_X86_REG_RAX);
    }
};
