# Ghidra MCP — Feature Wishlist

Generated from review of the Ghidra public Java API (`https://ghidra.re/ghidra_docs/api/`)
in the context of reversing Halo CE (halo1.dll) for the smc64 mod.

Existing tools covered: `decompile_function`, `disassemble_function`, `rename_function`,
`rename_variable`, `set_decompiler_comment`, `set_disassembly_comment`,
`set_function_prototype`, `set_local_variable_type`, `get_xrefs_to/from`,
`list_functions/namespaces/imports/exports/strings/data_items`, `search_functions_by_name`.

---

## 1. Struct CRUD  *(highest priority)*

**Rationale:** All Halo struct knowledge currently lives in CE Lua notes and this repo's
markdown files. If `FlareEntry`, `EntityRecord`, `FlareManager`, etc. were defined as
Ghidra `StructureDataType`s, the decompiler automatically emits
`entry->mountEntityHandle` instead of `*(uint *)(entry + 0x2c)` — every future
decompile in that codebase becomes readable without additional annotation.

**Desired tools:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `create_struct(name, size, category_path)` | `new StructureDataType(CategoryPath, name, size, dtm)` + `dtm.resolve()` | Create a new struct in the data type manager |
| `add_struct_field(struct_name, offset, type_str, field_name, comment)` | `Structure.insertAtOffset(offset, dataType, length, name, comment)` | Add a named field at a specific byte offset |
| `get_struct(name)` | `DataTypeManager.getDataType(DataTypePath)` + `Structure.getComponents()` | Return all fields with offsets, types, names |
| `rename_struct_field(struct_name, offset, new_name)` | `DataTypeComponent.setFieldName(name)` | Rename a field without removing and re-adding it |
| `delete_struct(name)` | `DataTypeManager.remove(DataType, TaskMonitor)` | Remove a struct definition |

**Notes:**
- `type_str` should accept Ghidra built-in names (`uint`, `int`, `float`, `ushort`, etc.)
  as well as names of previously defined structs (enabling nested/pointer types).
- `category_path` allows organizing types under namespaces like `/HaloCE/Flares/`.
- Pointer-to-struct fields need `PointerDataType(innerType, 8)` for 64-bit targets.

---

## 2. Apply Data Type at Address

**Rationale:** Defining a struct is only half the job. Without applying it to the global
variable, the decompiler still emits `DAT_7ffd7e6c0900` as an opaque longlong. Applying
`FlareManager *` at that address makes all reads/writes through it display as named field
accesses across the entire program.

**Desired tools:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `apply_datatype_at_address(address, type_str)` | `Listing.createData(addr, dataType)` | Apply a named type (or pointer-to-type) to a data address |
| `clear_datatype_at_address(address)` | `Listing.clearCodeUnits(addr, addr, false)` | Remove an applied type so it can be re-applied |

---

## 3. Enum CRUD

**Rationale:** Flag fields appear constantly in Halo internals (e.g., flare entry `+0x02`
flags: bit 1 = needs spatial update, bit 2 = in partition, bit 3 = lens flare confirmed).
Defining these as `EnumDataType`s and applying them means the decompiler emits
`flags & FLARE_IN_PARTITION` rather than `flags & 4`.

**Desired tools:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `create_enum(name, size_bytes, category_path)` | `new EnumDataType(CategoryPath, name, size, dtm)` | Create a new enum (size = 1, 2, or 4 bytes) |
| `add_enum_value(enum_name, value_name, value)` | `EnumDataType.add(name, value)` | Add a named constant |
| `get_enum(name)` | `DataTypeManager.getDataType()` + `Enum.getValues()` | List all named values |
| `apply_enum_at_address(address, enum_name)` | `Listing.createData(addr, enumType)` | Apply enum to a data location |

---

## 4. Label / Symbol Creation

**Rationale:** Many interesting addresses are not function entry points — globals, vtable
slots, data arrays, the start of a struct array. `rename_function` / `rename_data` only
work on existing symbols. There's no way to *create* a named label at an arbitrary
address, which limits annotation of data-heavy regions.

**Desired tools:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `create_label(address, name, namespace)` | `SymbolTable.createLabel(addr, name, namespace, SourceType.USER_DEFINED)` | Add a named symbol at any address |
| `delete_label(address, name)` | `Symbol.delete()` | Remove a user-defined label |

---

## 5. Function Parameter Rename + Retype

**Rationale:** `rename_variable` and `set_local_variable_type` cover local variables but
not parameters. Parameters are equally important for decompile readability — e.g., in
`updateFlareTransform(uint32_t flare_handle)`, the `param_1` name currently cannot be
changed without going through the full `set_function_prototype` string, which is fragile
and discards inferred type information.

**Desired tools:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `rename_parameter(func_address, param_index, new_name)` | `Parameter.setName(name, SourceType)` via `Function.updateFunction()` | Rename a specific parameter by 0-based index |
| `set_parameter_type(func_address, param_index, type_str)` | `Parameter.setDataType(dataType, SourceType)` | Override the inferred type of a parameter |

---

## 6. Bookmarks

**Rationale:** During active RE sessions it's useful to tag addresses with categories
like `HOOK_CANDIDATE`, `VERIFIED`, `NEEDS_REVIEW` without committing to a full rename
or comment. Bookmarks are lightweight, searchable in the Ghidra UI, and don't affect
the decompile output.

**Desired tools:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `set_bookmark(address, type, category, comment)` | `BookmarkManager.setBookmark(addr, type, category, comment)` | Create or update a bookmark |
| `get_bookmarks(address)` | `BookmarkManager.getBookmarks(addr)` | Read bookmarks at an address |
| `list_bookmarks(type)` | `BookmarkManager.getBookmarksIterator(type)` | List all bookmarks of a given type |
| `delete_bookmark(address, type, category)` | `Bookmark.delete()` | Remove a bookmark |

Standard Ghidra bookmark types: `"Note"`, `"Info"`, `"Warning"`, `"Error"`.
Custom types are also valid strings.

---

## 7. Get Bytes at Address

**Rationale:** Needed for signature generation from within Ghidra (to cross-validate
against CE's AOB scanner), verifying instruction bytes before annotating, and extracting
function prologues for a pattern library. Currently bytes are only visible embedded in
`disassemble_function` output, which is noisy for this purpose.

**Desired tool:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `get_bytes(address, length)` | `Memory.getBytes(addr, byte[])` | Return `length` bytes at `address` as a hex string |

---

## 8. Per-Instruction (EOL) Comment

**Rationale:** `set_disassembly_comment` and `set_decompiler_comment` operate at the
function level. A comment placed on the exact instruction where `*(entry + 0x2c)` is
loaded is far more precise than a block-level note — especially useful when a single
function has multiple struct accesses that need individual annotation.

**Desired tool:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `set_instruction_comment(address, comment, type)` | `CodeUnit.setComment(CodeUnit.EOL_COMMENT \| PLATE_COMMENT \| PRE_COMMENT, text)` | Set a comment on a specific instruction by address |

`type` values: `"eol"` (end-of-line), `"pre"` (above instruction), `"plate"` (banner before function), `"post"` (below instruction).

---

## 9. Export Struct Definitions as C Header

**Rationale:** The smc64 C++ hook code needs to reference Halo struct fields. Currently
this requires manually transcribing the struct from CE Lua notes into a `.h` file. If
Ghidra is the source of truth for struct definitions, a tool that emits a `.h` file
directly eliminates that round-trip and keeps the hook code in sync with the analysis.

**Desired tool:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `export_structs_as_header(category_path, output_path)` | Walk `DataTypeManager.getDataTypes(CategoryPath)`, emit `StructureDataType` → C struct text | Write a `.h` file for all structs under a category |

Example output:
```c
// Auto-generated from Ghidra — do not edit manually
// halo1.dll — HaloCE/Flares/

#pragma once
#include <stdint.h>

// FlareEntry — 0x7C bytes
struct FlareEntry {
    int16_t  generation;         // +0x00 non-zero = slot active
    uint16_t flags;              // +0x02 see FlareFlags enum
    uint32_t lightTagId;         // +0x04
    uint32_t lightRenderHandle;  // +0x08  -1 = none
    // ...
    uint32_t mountEntityHandle;  // +0x2C  0xFFFFFFFF = world-fixed
    // ...
};
```

---

## 10. Namespace / Class Creation

**Rationale:** `list_namespaces` exists but there's no way to create them. Grouping
functions under `FlareSystem::`, `EntityList::`, `Camera::` etc. makes large databases
navigable, gives the decompiler better context for inferred method calls, and mirrors
the logical structure of the game engine being reversed.

**Desired tools:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `create_namespace(name, parent_namespace)` | `SymbolTable.createNameSpace(parent, name, SourceType.USER_DEFINED)` | Create a plain namespace |
| `create_class(name, parent_namespace)` | `SymbolTable.createClass(parent, name, SourceType.USER_DEFINED)` | Create a class namespace (for grouping virtual methods) |
| `set_function_namespace(func_address, namespace)` | `Function.setParentNamespace(namespace)` | Move a function into a namespace |

---

## 11. Function Tags

**Rationale:** A lower-friction alternative to bookmarks for marking analysis state on
functions specifically. Tags like `"hook-candidate"`, `"layout-verified"`,
`"needs-callers"` are searchable in Ghidra's function filter and don't require choosing
a comment location.

**Desired tools:**

| Tool | Ghidra API | Description |
|------|-----------|-------------|
| `add_function_tag(func_address, tag_name)` | `FunctionTagManager.createFunctionTag(name, comment)` + `Function.addTag(tag)` | Tag a function |
| `remove_function_tag(func_address, tag_name)` | `Function.removeTag(tag)` | Remove a tag from a function |
| `list_functions_by_tag(tag_name)` | `FunctionTagManager.getFunctionsByTag(tag)` | Find all functions with a given tag |

---

## Priority Order Summary

| # | Feature | Impact | Complexity |
|---|---------|--------|------------|
| 1 | Struct CRUD | ★★★★★ | Medium |
| 2 | Apply datatype at address | ★★★★★ | Low |
| 3 | Enum CRUD | ★★★★☆ | Low |
| 4 | Label/symbol creation | ★★★★☆ | Low |
| 5 | Parameter rename + retype | ★★★★☆ | Low |
| 6 | Per-instruction EOL comment | ★★★☆☆ | Low |
| 7 | Bookmarks | ★★★☆☆ | Low |
| 8 | Get bytes at address | ★★★☆☆ | Low |
| 9 | Export structs as C header | ★★★☆☆ | Medium |
| 10 | Namespace/class creation | ★★☆☆☆ | Low |
| 11 | Function tags | ★★☆☆☆ | Low |

Items 1–3 together are the highest leverage: they turn Ghidra into a living type library
for the binary, making every subsequent decompile progressively cleaner.
