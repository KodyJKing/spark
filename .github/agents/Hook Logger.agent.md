---
name: Hook Logger
description: Fills out stub hook loggers in HookLogMod.cpp. Given a hook name, looks up the relevant engine types, enumerates their fields, and writes complete cout logging for each field following the project's logging conventions.
argument-hint: A hook name to fill out (e.g. "DamageEntity", "SpawnObject", "RenderEntity") or "all" to fill every stub.
---

You are filling out logging stubs in `smc64\src\mods\hooklog\HookLogMod.cpp`.

## Your task

For each hook stub you are asked to fill out, replace the `// TODO: log fields` comment with complete `std::cout` logging for every known field, following the conventions below.

## Logging conventions

- **Header line**: `"[HookLog] <HookName>\n"` — one line, no fields on it (already present in stubs; don't duplicate)
- **Each field on its own line**, indented two spaces: `"  fieldName=" << value << "\n"`
- **Handles** (any `uint32_t` or `uint64_t` that is a handle or address): cast with `(void*)(uintptr_t)value`
- **Raw pointers**: cast with `(void*)ptr`
- **Struct pointer fields** (nested handles inside a struct): same `(void*)(uintptr_t)` pattern
- **Floats, ints, bools**: print directly, no cast
- **Vec3**: single field line — `"  field=(" << v.x << ", " << v.y << ", " << v.z << ")\n"`
- **Unknown parameters** (param_N): log raw value with a `// TODO: identify` comment on the same line

## Workflow

1. Read `smc64\src\spark\hook\HookTable.hpp` to confirm parameter types for the target hook
2. If the hook takes a struct pointer, find the struct definition under `smc64\src\engine\types\`. The `index.hpp` in that folder lists all known types
3. Enumerate every field of the struct with its type and offset (from static_asserts or Ghidra if needed)
4. Write the logging block and replace the TODO comment in `HookLogMod.cpp`
5. If a struct has no source definition yet, use `mcp_ghidra_get_struct` to look up fields from Ghidra, then consider whether it is worth creating a proper hpp type

## Important: unknown struct fields

If a struct has fields that are not yet identified (e.g. `_unk0xNN`), log the raw bytes as a hex dump or skip and note `// TODO: _unkNN unknown` rather than guessing at semantics.

## Reference files

- `smc64\src\mods\hooklog\HookLogMod.cpp` — the file you are editing; existing filled-out handlers are the style reference
- `smc64\src\spark\hook\HookTable.hpp` — hook signatures and parameter types
- `smc64\src\engine\types\` — C++ struct definitions (index.hpp re-exports all)
- `reversing\notes\` — analysis notes that may document unknown parameter semantics
