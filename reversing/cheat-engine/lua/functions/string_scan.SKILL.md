---
name: string_scan
description: "Use when scanning a Cheat Engine / game process function for alpha-numeric string references found via 'lea' instructions. Loads and runs the string_scan Lua routine via dofile, then calls string_scan(address, max_depth). Use for: finding tag paths, asset names, or string literals embedded in a function; reverse-engineering Halo / MCC or similar game functions; discovering what strings a function (and its callees) reference."
argument-hint: "address expression, e.g. halo1.dll+B9F90A"
---

# string_scan Skill

Disassembles a function in the target process, walks every instruction up to a `ret`, and collects every predominantly alpha-numeric string that a `lea` instruction points to. Recurses into `call` targets up to `max_depth` levels deep.

## When to Use

- You have a function address and want to know what string literals it references.
- You are reverse-engineering tag/asset lookups in Halo MCC or similar games.
- You need a quick list of "what strings does this code touch?" without reading the full disassembly by hand.

## Key Facts

- The Lua file lives at `cheat-engine/lua/functions/string_scan.lua` (relative to the workspace root).
- `string_scan` is registered as a **global** by `dofile`; the helpers are local.
- `splitDisassembledString` returns values in order: **extra, opcode, bytes, address** (not the order stated in the CE docs — second return value is the opcode).
- A string qualifies as "predominantly alpha-numeric" when ≥ 75 % of its bytes are `[A-Za-z0-9]` and it is at least 3 characters long.
- `resolve_lea_target` first tries a bare hex parse, then falls back to CE's `getAddress()` with `errorOnLookupFailure(false)`.

## Procedure

1. **Load the script into CE**

   ```lua
   dofile([[C:\code\projects\hacking\smc64\cheat-engine\lua\functions\string_scan.lua]])
   ```

2. **Resolve the target address** (any CE expression works)

   ```lua
   local addr = getAddress("halo1.dll+B9F90A")
   -- or: local addr = 0x7FF89813F90A
   ```

3. **Run the scan**

   ```lua
   local results = string_scan(addr, 2)  -- depth=2 is usually enough
   ```

4. **Inspect results** — each entry is a table:

   | field     | type    | description                              |
   |-----------|---------|------------------------------------------|
   | `from`    | integer | address of the `lea` instruction         |
   | `address` | integer | address the `lea` points to (the string) |
   | `str`     | string  | null-terminated string read from there   |

   ```lua
   for i, r in ipairs(results) do
     print(string.format("[%d] 0x%X -> 0x%X  %s", i, r.from, r.address, r.str))
   end
   ```

5. **Full one-liner eval** (copy-paste into CE's Lua engine or via MCP `evaluate_lua`):

   ```lua
   dofile([[C:\code\projects\hacking\smc64\cheat-engine\lua\functions\string_scan.lua]])
   local res = string_scan(getAddress("halo1.dll+B9F90A"), 2)
   local lines = {"count=" .. #res}
   for i, r in ipairs(res) do
     lines[#lines+1] = i .. "|from=" .. string.format("%X", r.from)
                         .. "|ref="  .. string.format("%X", r.address)
                         .. "|"     .. r.str
   end
   return table.concat(lines, "\n")
   ```

## Parameters

| parameter   | default | notes                                                    |
|-------------|---------|----------------------------------------------------------|
| `address`   | —       | integer or CE symbol expression pre-resolved by caller   |
| `max_depth` | `2`     | recursion depth into `call` targets; 0 = current fn only |
| `visited`   | `{}`    | internal dedup table; pass `nil` on first call           |

## Caveats

- Indirect calls (`call qword ptr [...]`) are currently not followed; only direct/symbolic call targets that `getAddress` can resolve.
- The scan stops at the first `ret`; functions with multiple exit paths will only cover the first path.
- Increase the inner loop limit (default 2000 instructions) in the source file for very large functions.
