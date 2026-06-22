# Script System (HaloScript / BlamScript)

https://c20.reclaimers.net/h1/scripting/

The full HaloScript compiler, function table, and console variable system is **intact** in MCC's halo1.dll. Nothing relevant appears to have been stripped.

**Confirmed working expressions (via `_consoleSubmit`):**
- `(set rasterizer_wireframe true)` — variable setter
- `(unit_kill (unit (list_get (players) 1)))` — function calls, object casting, list indexing
- `(set game_speed_value 0.1)` - Did I waste a year on SUPERHOTMCC not using this???

Both the variable lookup (`_blamScriptOrConsoleStrings`) and the HaloScript function table are live.

---

## String Lookup Table

### `lookupBlamString` — `7fff49b505c8`
Two-phase lookup. Takes a `char*` name, returns `BlamStringSearchResult {index, flags0, flags1, flags2}`.

**Phase 1 — Engine strings** (`_blamScriptOrConsoleStrings` @ `7fff4a8d78e0`):
- 467 hardcoded entries, array of `char**`
- Hit: `index | 0x8000` (high bit set = negative when signed = "came from engine table")
- `flags0 = 0xFFFF`

**Phase 2 — Map-defined strings** (from loaded map):
- Count at `[DAT_7fff4ac85c78 + 0x4a8]`, base at `[DAT_7fff4ac85c78 + 0x4ac]`
- Stride: **0x5c bytes** per entry
- Relocation: `(ptr - MapBase) + RelocatedMapBase`
- Hit: `index & 0x7FFF` (high bit clear), all flags = 0

**Miss:** `index = -1`, all flags = 0.

### `_blamScriptOrConsoleStrings` — `7fff4a8d78e0`
Array of `char**` (pointer to pointer to string). 467 entries covering engine console/script variable names. Examples observed: `rasterizer_near_clip_distance`, `rasterizer_far_clip_distance`, `freeze_flying_camera`, `debug_no_drawing`, `console_dump_to_file`, `player_spawn_count`, etc.

> **TODO:** Determine if these are strictly HaloScript globals, console cvars, or both. Name kept vague (`_blamScriptOrConsoleStrings`) until confirmed.

---

## Console Input Pipeline

### `_consoleSubmit` — `7fff49b51470`
**Primary entry point for injecting console commands.** Signature: `void _consoleSubmit(char* rawInput)`.

There is no user-facing console UI in MCC Halo 1 (tilde does not work). Commands must be injected by calling this function directly.

Full pipeline:
```
rawInput
  → _normalizeConsoleInput          (wrap bare names in HaloScript syntax)
  → _hsBeginCompile(0)              (clear error state)
  → _hsCompileExpression(len, str, &node, &extra)   (parse + compile; returns node index, -1 on fail)
  → _hsExecuteExpression(nodeIdx)   (on success: evaluate)
  → _consoleReportError(0, node, extra, str)        (on failure: report error)
  → _hsEndCompile()                 (cleanup)
  → DAT_7fff4bb5101e = 0            (clear "input pending" flag)
```

Example inputs:
- `"rasterizer_wireframe"` → reads current value
- `"(set rasterizer_wireframe true)"` → sets variable
- `"(begin (set rasterizer_wireframe true) (set rasterizer_stats true))"` → multi-statement

### `_normalizeConsoleInput` — `7fff49b51278`
Takes raw console input (`char* input, char* output`). Strips whitespace/semicolons, then:

| Input form | Output |
|---|---|
| `varname` (known) | `(varname)` |
| `varname value` (known) | `(set varname value)` |
| `(expr1)(expr2)...` | `(begin ...)` |
| Unknown name | `(name)` — passed through as script call |

---

## HaloScript Compiler

### `_hsBeginCompile` — `7fff49c2854c`
Called at the start of each console submission. Likely clears error state / resets the syntax node pool. Single `int` parameter (always `0` from `_consoleSubmit`).

### `_hsCompileExpression` — `7fff49c286e4`
Parses and compiles a HaloScript expression string. Returns a node index (`int`), or `-1` on failure. Outputs two values (node pointer and extra) via out-params.

### `_hsExecuteExpression` — `7fff49afad70`
Evaluates a compiled expression by node index.

### `_hsEndCompile` — `7fff49c28628`
Cleanup after a compile cycle.

### `_consoleReportError` — `7fff49b50cdc`
Reports a compile error. Called only when compile fails but a partial node was produced.

### `_compileScriptVariableRef` — `7fff49c2a3d0`
Resolves a named variable reference inside a compiled script expression node. Calls `lookupBlamString` to locate engine variables or falls back to map script globals. Validates type compatibility. Sets resolved flags on the syntax node.

Error: `"this is not a valid variable name."`

### `_compileGlobalDeclaration` — `7fff49c294cc`
Compiles `(global <type> <name> <initial-value>)` declarations.
- Validates type string
- Checks name length (≤ 32 chars)
- Collision-checks against engine vars (`lookupBlamString`) and existing scripts

Errors: `"there is already a variable by this name."`, `"there is already a script by this name."`, `"this is not a valid type."`

### `_compileScriptDeclaration` — `7fff49c29800`
Compiles `(script <type> <name> <expressions>)` declarations.

Script types: `startup`, `dormant`, `continuous`, `static`, `stub` (and `local` variants).
- Validates name length (≤ 32 chars)
- Checks for name collisions via `lookupBlamString`
- Handles stub override: static scripts can override stub scripts of the same return type

Errors: `"script type must be \"startup\", \"dormant\", \"continuous\", or \"static\"."`, `"only static scripts of the same type can override stub scripts."`

---

## Key Data / Globals

| Symbol | Address | Notes |
|---|---|---|
| `DAT_7fff4ac85c78` | `7fff4ac85c78` | Base of map script data block. Offsets `+0x4a8` = script count, `+0x4ac` = script base ptr, `+0x4a0` = global base ptr |
| `DAT_MapBase` | TBD | Original map load address (for relocation) |
| `DAT_RelocatedMapBase` | TBD | Actual map load address |
| `DAT_7fff4abb60a8` | `7fff4abb60a8` | Guard: `-1` means map not loaded / system not ready |
| `DAT_7fff4be112b8` | `7fff4be112b8` | Script string heap base |
| `DAT_7fff4be112d8` | `7fff4be112d8` | Compiler error message pointer |
| `DAT_7fff4be112e0` | `7fff4be112e0` | Compiler error token |
| `PTR_s_unparsed_7fff4abad540` | `7fff4abad540` | Array of type name string pointers (indexed by type id). `unparsed` is type 0. |

---

## HaloScript `print` Builtin

### `hs_print_builtin` — `0xACA4AC` (VA: `7fff49afa4ac`)

The implementation of `(print <expr>)` in HaloScript.

**Signature:** `void(undefined8 unused, ulonglong script_thread_idx, char is_error)`

| Param | Role |
|---|---|
| `param_1` | Unused (padding / calling convention artifact) |
| `param_2` | Script thread index; lower 16 bits index into hs_thread_datum table at `DAT_7fff4ac65098` (stride `0x514`) |
| `param_3` | `'\0'` = normal eval; non-`'\0'` = error/type-mismatch path |

**Normal path (`param_3 == 0`):**

1. **`FUN_7fff49afc640(param_2, 4, 2, &local_428)`** — evaluates the argument expression; typed value returned via `local_428`
2. **Type ID resolution** — walks the HS syntax node table (`DAT_7fff4ac721a0`) via a chain of `ushort` dereferences starting from `expr[6]` (child index) to reach the argument's return type ID (`lVar3`)
3. **`hs_type_to_string_table[type_id](type_id, value, local_418, 0x400)`** — type-specific value→string converter; fills a 1024-byte stack buffer (`local_418`)
4. **`FUN_7fff49b61800(0, local_418)`** — outputs the string to the terminal, BUT only if:
   - `DAT__dev_mode_flag != 0` (probable developer/devmode flag), OR
   - `DAT__log_verbosity_level > 3` (probable verbosity/log level)
5. **`hs_thread_advance_pc(param_2)`** — advances the HS VM program counter (yield/step thread)

**Error path (`param_3 != 0`):** calls `FUN_7fff49afbdc0` to report a type-mismatch error back to the evaluator.

### `hs_type_to_string_table` — `DAT_7fff4a8b69a0`

Array of 8-byte function pointers, indexed by HaloScript type ID. Each entry is a function:
```
void fn(int type_id, value, char* out_buf, int buf_size)
```
Converts a typed HS value to its string representation. Called by `hs_print_builtin` before terminal output.

### `hs_thread_advance_pc` — `0xACBFAC` (VA: `7fff49afbfac`)

Advances the HS VM program counter for a thread. Called at the end of every builtin to yield execution.
Reads `expr[6]` (current PC expression index), follows to `expr[0x22]` (next expression), and writes it back to `expr[6]`.

### `FUN_7fff49b61800` — `0xB31800` (VA: `7fff49b61800`)

Terminal/console output sink. Takes `(int channel, char* message)`. Channel `0` = default.
Called conditionally from `hs_print_builtin` (gated by dev/verbosity flags) and unconditionally from `FUN_7fff49b50dd8`.

**Hook note:** To intercept all `(print ...)` output regardless of dev flags, hook `hs_print_builtin` and read `local_418` (rsp-relative stack buffer) after step 3. Alternatively, check whether `DAT_7fff4bedb940` and `DAT_7fff4ac710e7` are set at runtime in MCC's shipped config — if `FUN_7fff49b61800` is always reached, hooking it is simpler.

### Key addresses

| Symbol | Offset | VA | Notes |
|---|---|---|---|
| `hs_print_builtin` | `0xACA4AC` | `7fff49afa4ac` | Main hook target |
| `hs_type_to_string_table` | data | `7fff4a8b69a0` | fn ptr array by type ID |
| `hs_thread_advance_pc` | `0xACBFAC` | `7fff49afbfac` | HS VM PC advance |
| `FUN_7fff49b61800` | `0xB31800` | `7fff49b61800` | Terminal output sink |
| `DAT__dev_mode_flag` | data | `7fff4bedb940` | Probable dev mode flag (char, != 0) |
| `DAT__log_verbosity_level` | data | `7fff4ac710e7` | Probable verbosity level (byte, > 3 threshold) |

---

## HaloScript Function Table

Parallel to `_blamScriptOrConsoleStrings` (which holds variable/cvar names), HS **functions** (builtins) live in a separate table whose entries are small descriptors. The descriptor layout observed for `sound_impulse_start`:

| Offset | Type | Notes |
|---|---|---|
| `+0x00` | `char**` | name pointer (e.g. -> `"sound_impulse_start"`) |
| `+0x08` | `?`      | second slot — unknown role, possibly arg-type table or return-type descriptor |
| `+0x10` | `fn ptr` | builtin dispatcher (HS-side wrapper, see below) |

The dispatcher pointer is **not** the native implementation — it is a thin HS-VM trampoline (see "HS Builtin Dispatcher Pattern" below) that evaluates the script-side arguments and then forwards them to the real C++ function.

> The descriptor table is also referenced via `PTR_DAT_7fff507c60f0` from inside the dispatchers — `PTR_DAT_7fff507c60f0[builtin_index]` looks up the descriptor for the active builtin, and fields `+0x30` / `+0x32` carry the arg-type info passed to `FUN_7fff4f9fc870` (likely `hs_eval_args`).

### HS Builtin Dispatcher Pattern

All HS builtins reached through the function table share this shape (see `hs_print_builtin` and `_hs_sound_impulse_start_builtin`):

```c
void _hs_<name>_builtin(short builtin_index, uint32_t hs_thread_idx, char is_error) {
    auto* evaluated = FUN_7fff4f9fc870(
        hs_thread_idx,
        *(uint16_t*)(PTR_DAT_7fff507c60f0[builtin_index] + 0x30),  // arg type/count info
         (char*)   (PTR_DAT_7fff507c60f0[builtin_index] + 0x32),   // arg type list
        is_error);
    if (evaluated) {
        native_impl(evaluated[0], evaluated[1], evaluated[2], ...);
        hs_thread_advance_pc(hs_thread_idx, 0);
    }
}
```

The `evaluated` buffer holds the already-resolved, typed argument values (tag handles, object handles, ints, floats, ...). Hooking the **native impl** rather than the dispatcher gives clean, typed arguments without having to walk the HS VM state.

### Example: `sound_impulse_start`

The HS expression `(sound_impulse_start <sound-tag> <object> <scale>)` resolves to:

| Layer | Symbol | Address |
|---|---|---|
| Name string | `s_sound_impulse_start` | `7fff507bddb0` |
| Table entry | (descriptor) | `7fff507bf998` |
| Dispatcher | `_hs_sound_impulse_start_builtin` | `7fff4fa4e340` (offset `0xB1E340`) |
| Native impl | `soundImpulseStart` | `7fff4fa62f00` (offset `0xB32F00`) |

Native signature (confirmed):
```c
void soundImpulseStart(uint32_t soundTagHandle,
                       uint32_t sourceEntityHandle,  // 0xFFFFFFFF = sourceless
                       float    scale);              // clamped [0, DAT_7fff50730ef8]
```

Hooked in `smc64` as `Spark::SoundImpulseStart` (HookTable.hpp). The hook fires for every `(sound_impulse_start ...)` invocation regardless of whether the tag was successfully resolved upstream.

> **Note on "not a loaded tag" errors:** The tag lookup is performed by the HS argument evaluator (`FUN_7fff4f9fc870`) *before* the native impl is called. When a tag path is not in the cache, the dispatcher returns early and `soundImpulseStart` is never invoked. To diagnose missing tags, hook the upstream evaluator or watch `_consoleReportError`, not `soundImpulseStart`.

---

## Open Questions / Next Steps

- [ ] Write CE AA script to call `_consoleSubmit` from a hotkey with an arbitrary string
- [ ] Identify the console **display / render** function (terminal UI)
- [ ] Clarify whether `_blamScriptOrConsoleStrings` covers only engine globals or also console commands
- [ ] Check at runtime whether `DAT__dev_mode_flag` / `DAT__log_verbosity_level` are always set in shipped MCC — determines if `FUN_7fff49b61800` is a reliable hook target or if `hs_print_builtin` itself must be hooked
- [ ] Explore `FUN_7fff49afc640` — likely `hs_eval_expression(thread_idx, type, flags, out_value)` 
- [ ] Explore `FUN_7fff4f9fc870` — likely `hs_eval_args(thread_idx, arg_count, arg_types, is_error)` returning the resolved typed arg buffer used by all dispatchers
- [ ] Locate the HS function-table base — currently only individual descriptor entries are known (e.g. `7fff507bf998`). Walking the table would let us enumerate every builtin name + native impl in one pass.
- [ ] Explore `FUN_7fff49afbdc0` — error/type-mismatch reporter called from `hs_print_builtin` error path
- [ ] Map out `hs_type_to_string_table` entries — identify which type IDs correspond to int/real/string/boolean/unit etc.
- [ ] Resolve `DAT_MapBase` / `DAT_RelocatedMapBase` symbols
- [ ] Explore `FUN_7fff49b50508` (called in `_compileScriptVariableRef` to look up a script by index — likely `getScriptByIndex` or similar)
- [ ] Explore `FUN_7fff49b5031c` (called in `_compileGlobalDeclaration` and `_compileScriptDeclaration` — likely `registerScriptName`)
