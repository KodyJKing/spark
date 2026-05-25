# Script System (HaloScript / BlamScript)

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

## Open Questions / Next Steps

- [ ] Write CE AA script to call `_consoleSubmit` from a hotkey with an arbitrary string
- [ ] Identify the console **display / render** function (terminal UI)
- [ ] Clarify whether `_blamScriptOrConsoleStrings` covers only engine globals or also console commands
- [ ] Resolve `DAT_MapBase` / `DAT_RelocatedMapBase` symbols
- [ ] Explore `FUN_7fff49b50508` (called in `_compileScriptVariableRef` to look up a script by index — likely `getScriptByIndex` or similar)
- [ ] Explore `FUN_7fff49b5031c` (called in `_compileGlobalDeclaration` and `_compileScriptDeclaration` — likely `registerScriptName`)
