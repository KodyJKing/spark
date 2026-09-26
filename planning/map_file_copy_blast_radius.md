# Blast radius: MapFile abstraction & generic tag-copy

Companion to `planning/map_file_copy_scoping.md`. Enumerates the files/units
touched, split into **new**, **modified**, and **removed/obsolete**, plus
build/plumbing notes and a pre-flight verification list. Paths are relative to
`spark/src/` unless noted.

---

## 0. Ground rules / plumbing

- **Premake auto-includes sources.** `spark/premake5.lua` globs `src/**.hpp` /
  `src/**.cpp`, so new files under `src/` need no premake edit. New *projects*
  or vendored libs would; none are expected here.
- **Public API surface.** Functions exposed to mod DLLs (e.g. smc64) need
  `SPARK_API` (see `spark/SparkAPI.h`), matching `engine/tag.hpp` /
  `engine/map.hpp`. `getRuntimeMap` / `getSparkBuffers` / `getPersistedState`
  and the `MapFile` public methods fall here if mods call them.
- **Hooks are X-macros.** New engine detours are added to
  `spark/hook/HookTable.hpp` (the `HOOK(...)` table); handlers register via
  `Spark::<HookName>::addHandler`. The vertex/index *binding* detour needs a
  **new** table entry (address not yet reversed).
- **No shared-memory utility exists yet.** Grep found only read-only file
  mappings in unrelated vendor/test code. The named-shared-memory primitive for
  `SparkPersistedState` is net-new.

---

## 1. New files / units

### 1a. MapFile abstraction — `engine/map/`
- **`MapFile` interface** (base class): `Base` enum + `fromRelative`/`toRelative`,
  tag enumeration/find, `copyTag`, `allocateTag`, and the internal
  `copyTagData` / `copyBitmapData` / `copyVertexData` / `copyIndexData` virtuals.
  Likely reshapes the existing `map_file.hpp` and adds files, e.g.
  `raw_map_file.{hpp,cpp}` and `runtime_map_file.{hpp,cpp}`.
- **`RawMapFile`**: the on-disk implementation. Absorbs today's
  `Engine::Map::MapFile` (see §2a); read-only source; map/vertex/index-
  relative offsets.
- **`RuntimeMapFile`**: live-process implementation over `engine/map.hpp` +
  `engine/tag.hpp`. The only copier; side buffers resolve into `SparkBuffers`.
- **`getRuntimeMap()`** — cached, refreshed on scenario load.

### 1b. Copy engine
- `RuntimeMapFile::copyTag` + `copyTagData` and the per-resource copies. Schema-
  driven traversal (bulk-copy + fixups + base re-anchoring). No comparison —
  always rewrite.
- **`Engine::TagSchema::copyData`** (or equivalent traversal entry) in
  `engine/tags/schema/` — the shared walk used by copy; sibling to `claimBytes`.

### 1c. SparkBuffers — session-scoped
- `SparkBuffers` (texture/vertex/index, append-only) + `getSparkBuffers()`.
  Plain owned storage (e.g. `std::vector<uint8_t>` per buffer); rebuilt each
  injection; optionally leaked-on-uninject. New unit, e.g.
  `engine/spark_buffers.{hpp,cpp}`.

### 1d. Persistence primitive — new
- Named-shared-memory helper (create/open/keep-alive-across-uninject) — no
  existing util to reuse.
- `SparkPersistedState { void* tagResourcePathBlock; }` + `getPersistedState()`.
- Wire it into the tag-resource-string relocation for idempotency (see §2c).

### 1e. Engine resource detours
- **Texture redirect** handler on the existing `TextureCacheStartLoadingBitmap`
  / `CacheReadFile` hooks (§2f), + the CPU texture-reload trigger.
- **Vertex/index bind redirect** handler on a **new** hook (address TBD).
- Discriminator: "does this resource resolve into a Spark buffer" (range/marker).
  New module, e.g. `mods/.../spark_resources/` or under `engine/rendering/`.

### 1f. Schema distribution
- `/schemas` directory (loose JSON) shipped with Spark + loader with a dev
  env-var override. Replaces the single `./tag_schema.json` path in DissectTag
  (§2e). May add a `schemas/` loader unit near `engine/tags/schema/`.

---

## 2. Modified files / units

### 2a. `engine/map/map_file.{hpp,cpp}` — refactor into `RawMapFile`
- Today: standalone `Engine::Map::MapFile` (cache reader:
  `loadFromFile`, `translatePointer`, `translateVertexDataPointer`,
  `resolveBlock`, typed getters). Becomes `RawMapFile` behind the `MapFile`
  interface; add `Base`-relative translation; keep the `TagData::*` overlay
  structs (bitmap/model/vertex) for typed raw access (revisit vs. schema — §5).

### 2b. `engine/tags/schema/{schema.hpp,schema.cpp,schema_json.cpp}`
- Finalize the type-shape "nits."
- Add the copy traversal (`copyData`) alongside `claimBytes`/`structureClaimBytes`.
- Possibly change loading from single-file JSON to `/schemas` collection.

### 2c. `engine/tag.{hpp,cpp}` — tag-array growth idempotency
- `moveResourceStrings` has the explicit TODO ("Make this idempotent… associate
  data with a single map-session") — wire it to
  `SparkPersistedState.tagResourcePathBlock`.
- Surface `allocateTag` behind `RuntimeMapFile::allocateTag`; reconcile with the
  existing `Engine::allocateTag(CreateTagOptions)` / `allocateTagData` /
  `allocateTagPath`, `hasRoomForTag` / `makeRoomForTag`,
  `verifyResourceStringMonotonicity`, `guessTagSize`.

### 2d. `engine/map.{hpp,cpp}`
- Backing for `RuntimeMapFile` translation. Confirm `allocateMapMemory`
  (`Memory::allocBlockNear(mapRelocationOffset(), …)`) is process-lifetime, not
  DLL-scoped (§5). Possibly add a `Base`-aware translate helper.

### 2e. `mods/devtools/dissect/{DissectTag.cpp,DissectTag.hpp,types.hpp}`
- Adapt schema authoring/dissection to run over a `MapFile` (dissect `.map`
  tags, not just the runtime map).
- Migrate off `SCHEMA_FILE_PATH = "./tag_schema.json"` to the `/schemas`
  distribution (§1f). Touches `tagSchemas` global, `getTagSchema`,
  `saveSchemas` / `loadSchemas`.

### 2f. `spark/hook/HookTable.hpp` + handlers
- Give `TextureCacheStartLoadingBitmap` (0xB52104) / `CacheReadFile` (0xB7EC40)
  real handlers.
- **Add** a vertex/index buffer *binding* hook entry (reverse the address).

### 2g. `mods/spark-input/SparkInputMod.cpp`
- Contains commented-out prototype handlers for the two bitmap hooks. Move the
  real logic to the detour module (§1e) and delete the stubs.

### 2h. `mods/devtools/map_file/MapFileViewer.{cpp,hpp}`
- Standalone `.map` viewer that uses `Engine::Map::MapFile` directly.
  Repoint to the new `MapFile`/`RawMapFile` interface. Candidate host for the
  "browse a `.map`" side of the TagBrowser/DissectTag adaptation.

### 2i. `mods/devtools/TagBrowser.{cpp,hpp}`
- Adapt to browse via the `MapFile` abstraction (runtime and raw), per the
  scoping goal. Remove the `cloneTagNaive` path (§3).

### 2j. `mods/devtools/DevWindow.cpp`
- Window registry/includes. Drop the `InjectBitmap` include + UI entry (§3);
  add any new copy/inject UI entry point.

---

## 3. Remove / obsolete (cleanup)

- **`mods/devtools/tag_injection/InjectBitmap.{cpp,hpp}`** — explicitly
  "Abandoning this. Keeping briefly for reference," with a self-noted
  "WRONG: Cannot load relative to loadedMap." Fully superseded by
  `copyTag`/`copyBitmapData`. **Delete**, and remove its include + UI call from
  `DevWindow.cpp`. Its local `textureBuffer` / `copyTextureData` are subsumed by
  `SparkBuffers`; its hardcoded `d40.map` path goes away.
- **`cloneTagNaive`** (`engine/tag.hpp` / `tag.cpp`) and its only caller
  `TagBrowser.cpp:313` (behind `ENABLE_TAG_CLONING`) — superseded by generic
  `copyTag`. Remove once copy lands; drop the `ENABLE_TAG_CLONING` block.
- **`./tag_schema.json` single-file scheme** in DissectTag — replaced by the
  `/schemas` distribution.
- **Direct `Engine::Map::MapFile` usages** (`MapFileViewer.cpp`,
  `InjectBitmap.cpp`) — migrate to the interface (viewer) or delete (inject).

---

## 4. Not touched (adjacent, for reference)
- `engine/rendering/model_data.{cpp,hpp}` (`getModelDataPointer`) — relevant to
  the vertex/index detour but only *read*; likely referenced, not modified.
- `engine/halo1.hpp` — aggregate include; may add new headers to it.
- Other devtools (EngineInfo, InspectDX11, ScriptConsole) — unaffected.

---

## 5. Verify before/during (grounded unknowns)
1. **`allocateMapMemory` lifetime** — must survive DLL unload (engine
   dereferences moved strings / new tag data while uninjected). Confirm
   `Memory::allocBlockNear` is VirtualAlloc-backed / process-owned.
    - Can confirm this works.
2. **Named section survival across uninject** — leaked handle vs. launcher-held.
3. **Vertex/index binding hook** — address + signature need reversing (Ghidra).
    - Leave unimplemented stub for me.
4. **Texture CPU-reload mechanism** — confirm the existing method and where it
   hooks in for the patch path.
    - ```C++
    // Set the BitmapData's textureCache handle to 0xFFFFFFFF. (don't care about leaks during dev yet)
    BitmapData::markForReload();
    ```
5. **Resource-string monotonicity** — `hasRoomForTag` assumes the block starts
   at `tag[0]`'s path, contiguous/ascending; decide the failure behavior.
6. **`TagData::*` overlays vs. schema** — decide whether raw typed access stays
   or is fully schema-driven (affects how much of `map_file.hpp` remains).
    - For the most part, we're schema driven.
    - For touchups on types that need special buffer handling, cast to concrete types.