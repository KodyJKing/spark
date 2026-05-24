# Cheat Engine AA Hypothesis Test Scripts

Conventions for writing CE Auto-Assembler scripts that test static-analysis
hypotheses dynamically.  A test may work by **observing** (logging values to
confirm a field's role) or by **intervening** (patching a value, skipping a
branch, forcing a return) to confirm causality.  Both approaches are
first-class.  Scripts live in:

  reversing/cheat-engine/auto-assembler/tests/

File extension: `.CEA`

---

## Script Structure

```
// ============================================================
// TEST: <short title>
// ============================================================
//
// HYPOTHESIS
//   One paragraph.  State what field/behavior is under test,
//   what you expect it to be, and any prior evidence from
//   static analysis that motivates the test.
//
// METHOD
//   Hook address and why that point was chosen.
//   For instrumentation tests: list every field logged and its
//   offset/type.
//   For intervention tests: describe what is patched, what the
//   patched value forces, and what observable outcome confirms
//   or denies the hypothesis.
//
// PLAYER INSTRUCTIONS
//   1. Enable this script.
//   2. (Instrumentation) Add g_* names to the memory list
//      (they resolve by name).  Wait for g_hitCount to change
//      to confirm a fresh event fired before reading other slots.
//      (Intervention) Describe the observable in-game effect
//      the player should watch for.
//   3. Step-by-step in-game actions.  One action per trial.
//
// PREDICTIONS
//   Specific, falsifiable bullets.  Mark which prediction
//   anchors to prior static evidence vs. pure hypothesis.
//
// HOOK SAFETY NOTE
//   Bytes overwritten, register state, why it is safe.
//
// ============================================================

[ENABLE]

alloc(hookCode,   256, <near-address>)
alloc(g_foo,        4, <near-address>)
...
registersymbol(g_foo)
...

label(return)

hookCode:
  ...
  jmp return

<hook-address>:
  jmp hookCode
return:

[DISABLE]

<hook-address>:
  db <original bytes>

dealloc(hookCode)
unregistersymbol(g_foo)
dealloc(g_foo)
...
```

---

## Memory Allocation Rules

**Always provide a near-address to `alloc`.**  Without it CE may place the
block more than 2 GB from the hook, breaking RIP-relative encoding for
instructions like `mov [g_foo],eax`.

```
alloc(hookCode,   256, halo1.dll+B9EA28)   // code cave near hook
alloc(g_field,     4,  halo1.dll+B9EA28)   // data near code cave
```

**Use `alloc` + `registersymbol`, not `globalalloc`.**  `globalalloc` is
a single-call shorthand, but `alloc` + `registersymbol` is explicit and
easier to audit.  Always pair every `registersymbol` in `[ENABLE]` with
`unregistersymbol` + `dealloc` in `[DISABLE]`.

`hookCode` itself does not need `registersymbol` unless another script
references it by name.

---

## Always Log a Hit Counter

Include `g_hitCount` (DWORD) and `inc dword ptr [g_hitCount]` in every
hook.  It lets you confirm a new event actually fired before reading the
other slots, and gives you a rough call frequency.  This applies to
intervention scripts too — knowing the hook ran is essential when an
expected behavior change does *not* appear.

---

## Choosing a Hook Point

Prefer hooking **at function entry** to observe input parameters before
any are modified, or to intercept and overwrite them before they are used.
Hooking **mid-function** or **at a specific instruction** is appropriate
when you want to test the effect of a single branch, clamp, or write.
For Win64 calling convention:

- Param 1 → RCX
- Param 2 → RDX
- Param 3 → R8
- Param 4 → R9
- Stack params → `[RSP+0x28]`, `[RSP+0x30]`, …

For functions with a `MOV RAX,RSP` snapshot prologue (common in MSVC
x64), clobbering RAX *before* the original `MOV RAX,RSP` is safe because
that instruction overwrites RAX unconditionally.  Do **not** push/pop to
use the stack before `MOV RAX,RSP` executes — the snapshot captures the
pre-push RSP value and will be wrong.  Use global scratch variables
(`alloc(g_scratch, 8, ...)`) instead.

When hooking mid-function, choose an instruction boundary that is exactly
**5 bytes** so a relative JMP fits cleanly.  Verify byte count from the
Ghidra disassembly before writing the DISABLE restore bytes.

---

## Resolving Offsets

Use the offset script to turn a Ghidra name or VA into a current
module-relative offset:

```powershell
python .github/agents/scripts/ghidra_offset.py <name_or_VA> [...]
```

Use the result as `halo1.dll+<offset>` in the script.

---

## Documenting Results

After running a test, update the relevant note in `reversing/notes/`.

- Replace `[UNVERIFIED]` tags with confirmed values or a brief summary.
- If the result contradicts the hypothesis, describe *why* and what the
  revised interpretation is.  Do not just delete the old hypothesis.
- Short confirmed example from the sourceType test:
  - Field was expected to be a coarse category enum (bullet / melee /
    grenade / vehicle).  Observed values were per-weapon (PlasmaPistol =
    0x11, PlasmaRifle = 0x12), indicating it is a **weapon object-type
    index**, not a category enum.  The vehicle-seat check using value 4
    likely refers to a specific vehicle type ID, not a generic vehicle
    category.

---

## DISABLE Section

Always restore the exact original bytes:

```
<hook-address>:
  db <byte1> <byte2> ...   // must match what the JMP overwrote
```

Get the bytes from the Ghidra disassembly listing, not from memory, so
the restore is deterministic across sessions.
