# HaloScript Builtin Analysis Playbook

How to take a HaloScript function name (e.g. `sound_impulse_start`, `unit_kill`, `object_set_scale`) and turn it into a typed, hookable C++ symbol in `smc64`. Distilled from the `sound_impulse_start` walkthrough.

See also: [ScriptSystem.md](ScriptSystem.md) for the underlying HS function-table / dispatcher pattern.

---

## TL;DR pipeline

For each HS function name:

1. Find the **name string** in Ghidra (xrefs to known string addresses, not `list_strings`).
2. Follow the single data xref → lands on a **function-table descriptor entry**.
3. The descriptor entry is 3 pointers wide. The 3rd slot is the **dispatcher** (`_hs_<name>_builtin`).
4. Decompile the dispatcher. It always calls one inner native function with the args from `hs_eval_args` → that's the **native impl**.
5. Rename + retype the native impl, document, add a `HOOK( ... )` row, log in `HookLogMod`.

That's the whole loop. The rest of this doc is gotchas + conventions.

---

## Gotchas (learned the hard way)

### Do NOT call `mcp_ghidra_list_strings`
The `query` parameter is silently ignored. It dumps the entire string table (tens of thousands of entries) and will obliterate your context window. **Use xrefs from already-known string addresses instead.** If you need a fresh string by text, ask the user or have them point at the address in Ghidra.

### Image base shifts between sessions
halo1.dll is rebased frequently. Notes containing raw VAs (e.g. `0x7fff49...`) will be stale. **Always re-resolve via the offset script** and reference symbols by name:

```powershell
python .github/agents/scripts/ghidra_offset.py <symbolName>
```

Never embed raw VAs in Ghidra comments or `reversing/notes/`. Module-relative offsets (e.g. `0xB32F00`) are stable.

### The address you get from a "command string" reference is a STRING, not a function
If `mcp_ghidra_decompile_function_by_address(<addr>)` returns "No function found at or containing address", the address is data (the literal name string). Step back one level: call `mcp_ghidra_get_xrefs_to(<string_addr>)` → land in the descriptor table → the **third pointer** there is the function.

### Dispatcher ≠ implementation
Every HS builtin has a **2-layer** shape:
- `_hs_<name>_builtin(short builtinIndex, uint32_t hsThreadIdx, char isError)` — HS-VM trampoline
- `<name>(<typed args>)` — actual native function

Hook the **native impl**, not the dispatcher. The dispatcher's params are HS-VM internals (thread index, builtin slot index) — useless for our purposes. The native impl receives clean typed args (tag handles, entity handles, ints, floats).

### Errors like "not a loaded tag" happen UPSTREAM
The HS arg evaluator (`FUN_7fff4f9fc870`, likely `hs_eval_args`) does tag-by-path lookup before the dispatcher forwards to the impl. If a tag isn't in cache, **the impl is never called** — the dispatcher early-returns. Diagnose those via `_consoleReportError` (already hooked) or by inspecting the evaluator. The native-impl hook will not see them.

---

## Step-by-step recipe

### 1. Locate the name string
Two options:
- **You already have an xref hint** (e.g. user shows you a Ghidra screenshot): jump there directly.
- **You're starting cold**: find any *other* HS string whose address you know, look at its xref site (a descriptor table entry), then scan adjacent descriptors. The function table is contiguous — descriptors are at regular strides near each other.

### 2. Follow string → descriptor
```
mcp_ghidra_get_xrefs_to(<string_VA>)
```
Expect a single `[DATA]` xref. That's the descriptor's `+0x00` slot.

### 3. Read the descriptor
The 3-pointer entry layout is:

| Offset | Type | Role |
|---|---|---|
| `+0x00` | `char**` | name pointer (back to the string) |
| `+0x08` | `?` | second slot — role still unclear; possibly return-type / category |
| `+0x10` | function pointer | dispatcher (`_hs_<name>_builtin`) |

Use `mcp_ghidra_list_data_items(start=<xref-8>, end=<xref+24>)` if you need to read the bytes, or just open Ghidra and look. **Better: ask the user for a screenshot** — they're fast and unambiguous.

### 4. Decompile the dispatcher
```
mcp_ghidra_decompile_function_by_address(<dispatcher_VA>)
```
You'll see something like:

```c
void _hs_<name>_builtin(short builtinIndex, uint32_t hsThreadIdx, char isError) {
    auto* evaluatedArgs = FUN_<eval_args>(
        hsThreadIdx,
        *(uint16_t*)(PTR_DAT_7fff507c60f0[builtinIndex] + 0x30),
         (char*)   (PTR_DAT_7fff507c60f0[builtinIndex] + 0x32),
        isError);
    if (evaluatedArgs) {
        <native_impl>(evaluatedArgs[0], evaluatedArgs[1], ...);   // <-- THIS is the target
        hs_thread_advance_pc(hsThreadIdx, 0);
    }
}
```

The number of indexed reads off `evaluatedArgs` tells you the **arg count**. Their casts (raw `puVar1[i]` → uint, `(float)puVar1[i]` → float, etc.) hint at types.

### 5. Decompile the native impl
This is where the real semantics live. Look for:
- **Sentinel checks** like `if (param == 0xffffffff)` — that param is a handle (tag/entity).
- **`getTagDataPointer(handle)`** calls — confirms tag handle.
- **Clamping** (`if (0 <= x && x <= MAX)`) — confirms a normalized float/int.
- **Switches on the handle high byte** — usually entity vs. flare vs. projectile, etc.
- **Marker resolution** (`__doSomethingWithEntityAndMarker`) — sound/effect emit position.

Compare arg patterns to c20 docs (https://c20.reclaimers.net/h1/scripting/) to confirm names.

### 6. Rename in Ghidra
Use the confidence-prefix convention:

| Confidence | Prefix | Examples |
|---|---|---|
| High | _(none)_ | `soundImpulseStart`, `unitKill` |
| Medium | `_` | `_hs_<name>_builtin` (dispatcher — always medium because role is template-derived not analyzed) |
| Low | `__` | `__guessedHelper` |

For each function:
- `mcp_ghidra_rename_function_by_address` → name
- `mcp_ghidra_set_function_prototype` → typed signature
- `mcp_ghidra_rename_variable` for each `uVar1`/`pTVar1`/`local_XX` to readable names
- `mcp_ghidra_set_decompiler_comment` with: HS signature, param semantics, control-flow summary, hook target note

**Rename gotcha:** Ghidra renumbers anonymous vars (`uVar1` → `uVar2`) after each rename. Re-decompile to see fresh names, then rename the next batch. Don't try to batch them all blindly; about half will return "Variable not found".

### 7. Add the hook
Edit [HookTable.hpp](../../smc64/src/spark/hook/HookTable.hpp):

```cpp
HOOK( <Name>, <Ret>, <OffsetU>, <Args...> )
```

Get the offset:
```powershell
python .github/agents/scripts/ghidra_offset.py <symbolName>
```

The X-macro auto-wires it into `Spark::<Name>::addHandler` / `::original` and into `Mod::HookLog::toggles.<Name>`.

### 8. Add a log handler
Edit [HookLogMod.cpp](../../smc64/src/mods/hooklog/HookLogMod.cpp). Follow the existing pattern:

```cpp
Spark::<Name>::addHandler(modId_, +[](void*, auto next, <args>) -> <Ret> {
    if (Mod::HookLog::toggles.<Name> && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.<Name>))
        std::cout << "[HookLog] <Name>\n"
                  << "  <field>=" << <value> << "\n"
                  << tagInfoStr(<tagHandle>, "    ")   // for tag handle args
                  ;
    return next(<args>);
}, nullptr);
```

Use `tagInfoStr(handle)` for any tag handle — it prints resource path + group. This is huge for mining live tag handles from gameplay.

### 9. Build & commit
```powershell
.\scripts\build.ps1
```
Expect "Build succeeded. 0 Error(s)" before moving on.

### 10. Document in ScriptSystem.md
Append the new builtin to the table in [ScriptSystem.md](ScriptSystem.md) under a per-function subsection. Include:
- HS expression form (from c20)
- Name string VA (will go stale, but useful for current session)
- Module-relative offsets for dispatcher + impl (stable across sessions)
- Native signature
- Notable side effects / sentinel values

---

## Batch workflow suggestion

When asked to do many at once:

1. **Ask the user for a list of target HS function names up front.** Don't try to walk the function table cold — it's much faster to grep c20 + their concrete needs.
2. **Process them in small batches (3–5).** Each one is a sequence of ~10 Ghidra calls; doing 50 in one shot will blow context.
3. **Group by category** when possible (`sound_*`, `unit_*`, `object_*`) — adjacent table entries often share patterns and you can recycle struct knowledge.
4. **Stub the hook table aggressively, fill in logging incrementally.** Adding `HOOK( ... )` rows is cheap; writing detailed logs takes thought. Get the rows in place first so toggles appear in the UI, then flesh out logging per row as needed.
5. **After each batch, build and commit.** Don't accumulate uncompiled changes — easier to bisect failures.

---

## Open questions / nice-to-haves

- [ ] Find the HS function-table **base address** so we can enumerate all builtins in one pass instead of name-by-name xref walking. Once located, a Ghidra script could auto-name every `_hs_<name>_builtin` dispatcher from its descriptor's string pointer.
- [ ] Confirm the role of the `+0x08` slot in each descriptor (return type? category? jump table for varargs?).
- [ ] Identify `FUN_7fff4f9fc870` / `hs_eval_args` formally — needed for any work on error-path interception or arg-time tag substitution.
- [ ] Consider a Ghidra script that, given a string VA, auto-creates the dispatcher rename + a stub `HOOK( ... )` line for `HookTable.hpp` to paste in.
