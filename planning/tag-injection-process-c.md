# Process C: checked static tag injection

Fork of [tag-injection.md](tag-injection.md). The original is retained as historical
design context; the old C++ implementation is not an acceptance reference.

## Status and contract

This is an experimental offline serializer, not a protected-map-check bypass.
Passing its tests does not establish that MCC CEA accepts the resulting maps.
No installed maps are modified by the tests.

`Engine::Map::Spark::injectTags(hostBytes, guestBytes, tags, schema)` returns an
owning byte vector. Both inputs remain unchanged. Invalid or unsupported input
throws `std::runtime_error` with an `injectTags:` diagnostic; no partial result is
published. Sizes are taken from bounded spans, not trusted raw pointers.

Supported inputs are uncompressed, little-endian MCC CEA version-13 raw maps with
zero `paddingLength`, a final tag-data section, and a contiguous model section
preceding it (at most three zero alignment bytes may separate them).
Schemas may be sparse, and tag roots may leave `size` at zero, as the existing
dissector schemas do. For those roots the serializer bounds-checks the extent of
the mapped fields; it does not guess the complete payload size. All metadata
backing bytes, including gaps, are copied unchanged unless a fixup targets them.
Block elements and embedded structures still require explicit nonzero sizes
because their strides/extents cannot safely be inferred from field placement.

Every relocation-sensitive field must be mapped, except fields covered by the
explicit bitmap/model/BSP handlers. Unmapped gaps and scalar fields must not hide
additional pointers or tag handles. Structural validation cannot establish this
semantic completeness, and gaps alone do not demonstrate an incomplete mapping.

## Process

1. Validate headers, section bounds, tag identities and terminated names.
   Preserve host indices and handles. Resolve requested tags by `(group, path)`.
   Prefer an existing host tag with that identity; otherwise assign a new index,
   preserving the guest handle's salt. Traverse dependencies to a fixed point,
   including cyclic references. A stale guest handle or missing guest dependency
   is an error. Host handles are preserved without re-resolving them: compiled
   maps may retain editor-only references to tags excluded from the cache.
2. Walk schema-described host and imported guest metadata. Record fixups using
   section-relative integer locations, never pointers into growable buffers.
   Check block extents, schema field bounds/overlaps, fixups and pointer ranges.
   Intern names by their full string, not a hash alone.
3. Build the output in this order:
   - Host fixed prefix, ending at the old model-data offset.
   - Imported bitmap payloads (four-byte aligned).
   - Host vertices followed by guest vertices, then host indices followed by
     guest indices. Guest model buffers are copied in full only when a model is
     imported; unused bytes are retained rather than guessing ownership.
   - Alignment padding, new tag-data header, combined tag array, interned names.
   - Aligned copies of the original host tag-data section and, when needed,
     guest tag-data section as metadata backing storage.
4. Link names, metadata pointers, handles and bitmap offsets. Guest model offsets
   are displaced within their respective vertex/index buffers. Host model offsets
   do not change relative to those buffers. Update counts and all section/file
   sizes only after the final layout is known.

The backing copies deliberately retain obsolete headers, tables, names and unused
guest data. Only the new table is active. This costs space but preserves shared
metadata ranges and avoids assuming that tag payloads are ordered or disjoint.
This layout choice needs explicit MCC acceptance testing.

Encoded tag addresses use `0x50000000 + fileOffset - tagDataOffset`. A file
displacement alone is not a valid pointer delta.

## BSPs and resource restrictions

- Host BSP tag data pointers remain zero. BSP payloads stay in the fixed prefix;
  their block/data pointers use the scenario's BSP runtime-address base and are
  not relocated. Their tag-reference names are linked to the new name table.
- Imported scenarios and BSPs are rejected, unless resolved to an existing host
  tag before import. Adding BSPs requires a separate scenario/BSP layout design.
- Inline DataPointer payloads are supported using the 20-byte disk layout, not
  the native pointer-sized C++ structure. Nonempty payloads with nonzero flags or
  null data addresses take separate host/guest external-data paths.
  Host descriptors remain byte-for-byte unchanged without reading their payloads
  or validating external offsets against this map. External offsets may exceed
  the entire map size (observed for sound samples); they cannot all be treated
  as offsets into the fixed raw-data section. Successful serialization emits one
  warning that payload storage and resource compatibility have not been verified.
  Guest external payloads fail explicitly: relocation is not implemented.
  Inline payloads may retain nonzero source-file offsets; those offsets are
  preserved while the validated metadata address is relocated.
- Internal bitmap payloads must reside in the source map's fixed prefix.
  Host external bitmap records are preserved unchanged, including their flags,
  cache offsets, sizes and handles; their pixel data is neither read nor copied.
  The output therefore requires the same compatible shared bitmap cache as the
  host. Missing, changed or corrupt cache contents are not validated by injection.
  Guest external bitmaps are allowed under the same-cache assumption: their
  flags, cache offsets and sizes are preserved, and their owner tag handles are
  remapped. A successful import emits one warning per injection that cache
  compatibility has not been verified. Spool displays it in a warning dialog and
  the saved-result status; API callers can collect warnings or use default logging.
  Future verification can compare the installed cache files against checksums
  distributed by mod authors; no checksum validation is implemented yet.
  Model buffers must be present in the source map.
  Nonzero runtime-initialized model vertex pointers are rejected.
- Empty blocks/data and null tag references retain their sentinel values.
- Missing/empty root schemas, unspecified block strides, unexpected section layouts, invalid indices,
  overflow and conflicting aliases are errors, not best-effort output.
- Existing checksum fields are preserved. Their recomputation/acceptance semantics
  have not been established; the serializer does not claim runtime compatibility.

## Validation and remaining acceptance work

Synthetic Catch2 tests cover zero-injection rebuilding, dependencies and cycles,
name/metadata relocation, inline data, resource buffers, BSP-local pointers,
identity reuse, malformed inputs and failure atomicity. They require no game files.

From the Spark repository root:

```powershell
premake5.exe vs2022
MSBuild.exe spark.sln /t:spark-test /p:Configuration=Debug /p:Platform=Win64
.\bin\Debug-Win64\spark-test\spark-test.exe "[inject_tags]"
```

Tests include loading the actual bundled `scnr` schema, traversing its zero-size
root and relocating its mapped references/data while preserving unmapped bytes.
The raw header signatures, version and model/tag section boundary were checked
against local backups. The tag-data header's scenario handle is at `+0x04` and
checksum at `+0x08`; real-map tests prevent regression to the originally swapped
field definitions.
The bundled `Struct_scnr` has 54 mapped fields and an unspecified root size.
That is supported, not evidence of an absent scenario mapping.

### Real-map regression tests

Set `MCC_HOME` to the MCC installation and run:

```powershell
.\bin\Debug-Win64\spark-test\spark-test.exe "[.real_maps]"
```

These opt-in tests read `halo1\maps_backup`. They validate guest-map listing for
level maps (excluding the separate `bitmaps.map` resource archive), and attempt
to inject `weapons\shotgun\shotgun` (`mod2`) from `d40.map` into `b30.map`.
The result is kept in memory. Neither inputs nor any existing output files are
written; inputs are reread and compared even when injection throws.

The shotgun test currently exposes a real schema blocker, rather than accepting
partial output: `StartingLocations` declares a 28-byte stride but maps a 16-byte
`field_0x1C` at offset 28. It remains a failing opt-in regression until the mapping
is corrected. Beyond that, 51 groups present in the tested `b30` lack nonempty
root schemas (including `lens`, `matg`, `scen`, `coll`, and `antr`). Host relocation
requires those mappings; passing synthetic tests is not campaign-map readiness.
No unsafe pointer guessing or skipping of those host groups is performed.

Before using an output in MCC:

1. Supply complete, semantically correct schemas for every visited tag group.
2. Establish the protected-map invariants and checksum requirements.
3. Serialize a zero-injection map to a separate output directory; reopen and
   inspect it, then load it in an explicitly managed test installation.
4. Repeat with one bitmap/model dependency chain; test rendering, audio and BSP
   transitions. Non-contiguous bitmap storage still needs runtime confirmation.
5. Add verified external DataPointer/resource-cache formats and their fixtures.

## Spool UI

Open a host map normally, then use **File > Open Guest Map for Injection...**
or **Tools > Tag Injection**. The guest is validated before listing owned tag
identities; it does not replace or normalize the host map. The picker supports
group/path filtering, persistent checkbox selection, selecting filtered results,
and clearing the selection.

**Inject Selected and Save As...** opens a native Save As dialog with a distinct
`<host>.injected.map` suggestion. Cancellation returns without injecting or writing.
The injector uses raw input bytes, not the dissector's normalized or edited buffer.
Opening another host clears the guest and selection.

Saving checks filesystem identity to reject host/guest aliases, holds input handles
that deny writes/deletion, and writes/flushes a same-directory temporary file before
replacing the chosen output. Read, dialog, injection and save failures are reported
to the user. No partially written result is published.

File-open detours and automatic game-map deployment remain outside this offline
workflow.
