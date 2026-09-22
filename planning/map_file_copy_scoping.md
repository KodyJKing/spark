# Scoping: MapFile abstraction & generic tag-copy

Status: draft for review
Owner: —
Related: `planning/todos.md` ("MapFile work"), `engine/map_file/`, `engine/tags/schema/`, `mods/devtools/dissect/`

> This document is intentionally written at the level of **capabilities and
> responsibilities**, not concrete signatures. The exact shape of the existing
> `Engine::TagSchema` and `Engine::MapFile` types is expected to change (there
> are outstanding nits to resolve first), so the plan below refers to those
> types by the *role* they play rather than their current fields. Any code
> shown is illustrative pseudocode, not a committed API.

---

## 1. Goal

Be able to pull tag data — starting with **bitmaps** and **gbxmodels** — out of
an arbitrary Halo 1 `.map` file *by tag name* and make it usable in the running
game, without hard-coding map offsets. The end state is a single, schema-driven
copy routine that runs on the **live map** (`RuntimeMapFile`) and pulls tag data
*from* a source map (typically a `RawMapFile`) into the running game, allocating
any texture/vertex/index data into **Spark-managed buffers**.

Because this runs in an injected DLL that is repeatedly uninjected and
re-injected during development, the whole pipeline must be **idempotent** and
must **gracefully survive re-injection** (see §3.4). That constraint shapes the
design as much as the copy mechanics do.

### Motivating use cases (from `todos.md`)

1. Load a bitmap from a `.map` file by tag name and render it in-game.
2. Load a gbxmodel from a `.map` file by tag name.
3. Adapt the TagBrowser / TagDissect devtools to operate over `.map` files, not
   just the currently loaded map.

---

## 2. Current state (what exists today)

Two independent access paths exist, with no shared abstraction:

- **On-disk cache access** — `Engine::MapFile::MapFile`
  (`engine/map_file/map_file.hpp`). Loads a `.map` from a file buffer and
  resolves the in-file pointer scheme (`translatePointer`,
  `translateVertexDataPointer`, `resolveBlock`, typed getters for effect /
  model / bitmap). Uses standalone overlay structs, deliberately *not* shared
  with the live-process types.

- **Live in-process access** — `engine/map.hpp` + `engine/tag.hpp`.
  `translateMapAddress` / `translateToMapAddress`, `getTag`, `findTag`,
  `BlockPointer::get`, `allocateTag`/`allocateTagData`, `cloneTagNaive`. Works
  against the map the engine currently has loaded.

- **Schema system** — `Engine::TagSchema` (`engine/tags/schema/`). Describes
  tag layouts (structures, fields, primitive types, enumerations), serializes
  to/from JSON, and can walk a structure to "claim" the bytes it covers
  (`claimBytes`) — this is the seed of schema-driven sizing/traversal. Authored
  interactively by the `DissectTag` devtool, currently persisted to a single
  loose `./tag_schema.json`.

**Gap:** the two access paths use different pointer conventions and different
struct definitions, and neither is driven by the schema. There is no shared
notion of "which relative base is this offset expressed against," and no
generic, schema-driven copy. Copying today means hand-written, per-tag-type
code (e.g. `cloneTagNaive`).

---

## 3. Concepts to pin down

These are the ideas the design hinges on. Getting the vocabulary crisp is more
important right now than the struct fields.

### 3.1 Relative-address bases

`todos.md` already identifies that offsets in tag data are expressed against
several distinct bases, and these are *not* currently modeled cleanly:

- **Map-relative** — offset from the start of the map/cache.
- **Tag-array-relative** — offset within the tag data blob.
- **Model-data-relative**, which subdivides into:
  - **Vertex-data-relative**
  - **Index-data-relative** (immediately follows vertex data in a raw cache
    file, but *not* necessarily in the runtime layout).

The abstraction needs a first-class way to say "translate this pointer, which
is expressed against base X" and "give me the offset of this pointer relative
to base X," for both the raw and runtime representations. The raw and runtime
formats disagree on how these bases are laid out (esp. index vs. vertex data
and texture data), so translation must be per-representation.

### 3.2 Schema-driven traversal & sizing

Accurately measuring and walking tag data requires a schema per tag type. The
`claimBytes` traversal already demonstrates the mechanic (recurse structures,
follow block pointers, mark covered bytes). Copy is the same traversal with a
different action at each node.

Decisions needed:
- Which tag types must have schemas for the first milestone (bitmap, gbxmodel,
  and their transitive dependencies).
- How complete/trustworthy the existing authored schemas are, and what cleanup
  they need.

A tag can only be copied/patched if its schema is known. A missing schema
**anywhere in the transitive closure hard-blocks the copy** — no partial or
blind copying.

### 3.3 Copy semantics (no comparison, always copy)

Copy is **target-driven**: `RuntimeMapFile::copyTag(sourceMap, sourceHandle)`
runs on the destination (the live map) and pulls from `sourceMap`. Only the
runtime implementation copies; `RawMapFile` is read-only source (see §10).

There is **no comparison**: copy always writes, even for unchanged data.
Idempotency comes from determinism — re-copying the same source yields the same
bytes, so a re-inject re-establishes the same end state. This is simpler than
reconciliation and needs no ownership bookkeeping or schema-normalized compare;
the cost is re-copying on every inject, accepted as a dev/load-time cost and
optimizable later if it proves slow.

Per tag, matched between maps by `(groupId, path)`:
- **Top-level tag** (the one the mod asked to copy) → always (re)copied/patched.
- **Referenced tag, absent in target** → copied, then recurse into its references.
- **Referenced tag, already present** → referenced as-is, not re-copied.
- On patch: leave tag data in place if the size matches, reallocate if it differs.

> **Open gap — dependency staleness.** Because side buffers are session-scoped
> (§3.3a) and referenced tags are only copied when *absent*, a dependency Spark
> created in a previous session is *present* on re-inject but its buffer data is
> gone — it resolves its Spark-buffer offset against this session's empty buffer,
> yielding garbage, and patching the top-level tag does not refresh a
> dependency's buffers. Closing this means always re-copying the declared closure
> (not "if absent"); deferred while near-term targets are shallow. See §9.

The copy/patch traversal itself should:
- Traverse the source tag via its schema.
- Bulk-copy structures (not field-by-field) where layouts match.
- Apply **fixups** after the bulk copy for anything that can't be copied
  verbatim: relative pointers (block pointers, texture/vertex/index data),
  cross-tag references, handles/IDs that must be re-issued in the destination.
- Translate every relative pointer through the base model in §3.1: compute
  `sourcePtr - sourceBase`, then re-anchor onto `destBase`.
- Return a ready-to-use tag handle in the destination.

Patching tag data does not by itself refresh engine/GPU-derived state, but that
is handled: Spark manages the vertex/index buffers it detours the *binding* to,
and forces texture reloads from CPU via an existing mechanism (§7).

Bitmap pixel data and model vertex/index data are the notable "side buffers"
that live outside the tag-data blob and need representation-specific handling
(raw map → map/vertex/index-relative offset; runtime → a **Spark-managed
buffer**, §3.3a).

### 3.3a Spark-managed buffers (`SparkBuffers`)

At runtime, copied side-buffer data does not go into the engine's map memory —
it goes into Spark-owned buffers for texture, vertex, and index data. The engine
is then detoured to consume them:

- **Texture** buffer *mapping/loading* is detoured (redirect when the resource
  resolves into a Spark buffer).
- **Vertex/index** buffer *binding* is detoured (not mapping/loading).
- `copyBitmapData` / `copyVertexData` / `copyIndexData` return offsets into
  these buffers (whereas `RawMapFile` would return map/vertex/index-relative
  offsets).

Tags are never removed, so the buffers are **append-only** and hand out fixed
offsets. Detours **re-resolve offset→pointer at every use** (an append may move
the backing store; offsets are stable, raw pointers are not), and each appended
block is **padded/aligned** for GPU vertex/index binding and texture rows.

`SparkBuffers` is **not persisted** — it is session-scoped, rebuilt each
injection (content re-copied per §3.3) and released on uninject. That removes the
earlier constraints entirely: no shared-memory placement, no STL-ABI concern, no
"outlive DLL unload" requirement — so `SparkBuffers` can be plain owned storage
(e.g. a `std::vector<uint8_t>` per buffer). One nuance: to preserve the tested
"ugly but doesn't crash" state while uninjected, buffers may still be **leaked**
(abandoned, not freed) on uninject so their addresses aren't immediately reused
— a per-session leak that dies on process exit.

### 3.4 Inject / re-inject lifecycle & idempotency

This is a first-class requirement, not a nicety. During development the DLL is
repeatedly uninjected and re-injected against a running game, and the pipeline
must tolerate that:

- **Persisted state is minimal** — `getPersistedState()` returns a
  `SparkPersistedState` backed by **named shared memory**; it survives
  uninject/re-inject cycles but *not* process exit. It no longer holds buffers.
  Its one job is recording where the tag-resource-path block was relocated
  (`tagResourcePathBlock`) so that relocation happens once and idempotently —
  see §3.4a.
- **Idempotent copy** — no bookkeeping needed: copy is deterministic and always
  rewrites (§3.3), so a re-inject with no source changes re-produces the same
  bytes. `(groupId, path)` matching locates the target and prevents duplicate
  tag-array entries (present ⇒ patch, not add).
- **Shared-section survival** — the named section holding `SparkPersistedState`
  must survive the uninject gap: leak the section handle (handles are
  process-owned, so they outlive DLL unload) or have the launcher hold one.
- **New tags left in place at uninject time** — copied tags/data are not torn
  down on uninjection.
- **Graceful while-uninjected behavior** — custom tags point at junk data while
  uninjected; tested to be visually ugly but *not* a crash, so acceptable
  between sessions.

### 3.4a Growing the tag array (`allocateTag` + string relocation)

Adding a genuinely new tag needs a free slot at the end of the fixed-position
tag array, but the block of tag-resource-path strings sits immediately after it.
`RuntimeMapFile::allocateTag` makes room by **relocating that string block**
(measure total length → allocate via `allocateMapMemory` → copy strings → repoint
every tag's `resourcePathAddress`), freeing the original region for the tag array
to grow into *in place* — the array base can't move without breaking every
`tagID → address` computation. Current implementation: `moveResourceStrings` /
`makeRoomForTag` in [engine/tag.cpp](engine/tag.cpp).

Idempotency has two independent guards:
- The **string move happens once per map-session**, guarded by
  `tagResourcePathBlock`: if set, the block is already relocated → skip. (A naive
  re-run would allocate a second block, leak the first, and double-repoint.)
- **Tag addition** is guarded by `(groupId, path)` presence: a tag present from a
  previous session is patched, not re-added, so it consumes no slot. The freed
  region is therefore a fixed budget spent only by *genuinely new* distinct tags
  across the map-session's life, not per inject.

---

## 4. Proposed architecture (decoupled)

A single polymorphic `MapFile` role with two implementations, plus a schema
collection accessor. **Names and signatures below are placeholders** pending
the type nits.

- `MapFile` (interface / role):
  - Translate to/from a relative base (the §3.1 base enum).
  - Enumerate/find tags (by name/path, by group).
  - `copyTag(sourceMap, sourceHandle) -> destHandle` — the public entry point,
    implemented on the **target** map; orchestrates dependency copy, data copy,
    and fixups. No comparison — always rewrites (§3.3); `(groupId, path)` locates
    the target. Refuses if any schema in the closure
    is missing.
  - Internal, representation-specific side-buffer copy: `copyTagData(...)`,
    `copyBitmapData(...)`, `copyVertexData(...)`, `copyIndexData(...)`.
  - `allocateTag() -> destHandle` — add a new entry to the target tag array;
    on `RuntimeMapFile` this relocates the resource-string block to make room,
    idempotently (§3.4a).

- `RawMapFile : MapFile` — backed by a file buffer (folds in today's
  `Engine::MapFile::MapFile` capabilities). **Read-only source**: offsets are
  map/vertex/index-relative; copy methods are not implemented here.

- `RuntimeMapFile : MapFile` — backed by the live process (folds in today's
  `engine/map.hpp` + `engine/tag.hpp` access). **The only implementation that
  copies.** Side buffers resolve into `SparkBuffers` (§3.3a) rather than
  map-relative offsets.

- `getRuntimeMap()` — returns the cached runtime map, refreshed on scenario
  load.

- `getSparkBuffers()` — returns the Spark-managed texture/vertex/index buffers.
  Session-scoped (not persisted); rebuilt each injection (§3.3a).

- `getPersistedState()` — returns the named-shared-memory `SparkPersistedState`
  (records `tagResourcePathBlock` for idempotent string relocation, §3.4a);
  survives re-injection, not process exit.

- `getTagSchemas()` — returns the global schema collection, loaded from a
  `/schemas` location (see §6).

The key design property: **the copy traversal is written once against the
`MapFile` interface and the schema**, and only base-translation + side-buffer
placement differ per source/target. The `RawMapFile` → `RuntimeMapFile`
direction is the one we implement first (and, per current plan, the only one).

---

## 5. Work breakdown

Phased so that value lands early and each phase de-risks the next. Phases are
sequenced; within a phase, items can be parallelized.

### Phase 0 — Prerequisite decisions (blocking, owned by you)
- Finalize the shape of the `TagSchema` types (the nits).
- Finalize the shape of the `MapFile` interface + base enum.
- Decide the `SparkPersistedState` shape (`tagResourcePathBlock`) and the
  named-shared-memory scheme (§3.4).
- Decide schema distribution & authoring workflow (§6).
- Decide the engine resource-detour mechanism (§7).
- Output: agreed interfaces. Everything below targets those.

### Phase 1 — Relative-address model
- Introduce the base enum and to/from-relative translation on the `MapFile`
  interface.
- Implement translation for `RawMapFile` (port existing `translatePointer` /
  `translateVertexDataPointer` behavior behind the new interface).
- Implement translation for `RuntimeMapFile` (wrap `translateMapAddress` /
  `translateToMapAddress`).
- Milestone: both implementations can round-trip pointers for map/tag/vertex/
  index bases, covered by unit tests against a known map.

### Phase 2 — Schema coverage for the first targets
- Audit/clean the existing schemas.
- Ensure bitmap + gbxmodel and their transitive dependencies have correct,
  size-accurate schemas.
- Milestone: `claimBytes`-style traversal fully covers a bitmap and a gbxmodel
  with no unclaimed/over-claimed bytes.

### Phase 3 — Tag-array growth + session buffers + lifecycle foundation
- Make `allocateTag`'s resource-string relocation idempotent via
  `SparkPersistedState.tagResourcePathBlock` in named shared memory, keeping the
  section alive across the uninject gap (§3.4, §3.4a). Builds on
  `moveResourceStrings` / `makeRoomForTag` in [engine/tag.cpp](engine/tag.cpp).
- Stand up the session-scoped Spark texture/vertex/index buffers (append-only,
  rebuilt each injection; §3.3a).
- Establish the copy primitives: `(groupId, path)` lookup in the target map.
- Milestone: add a new tag across an uninject/re-inject cycle without
  double-moving the string block or duplicating the tag.

### Phase 4 — Generic tag-data copy
- Implement schema-driven `copyTagData` on `RuntimeMapFile` (bulk copy +
  block-pointer fixups + base re-anchoring) for `RawMapFile → RuntimeMapFile`.
- Implement recursive dependency copy (top-level always patched; referenced tags
  copied when absent) with **no comparison — always rewrite** (§3.3), plus a
  small set of custom field fixups (handles/IDs, cross-tag refs). Hard-refuse if
  any schema in the closure is missing.
- Verify idempotency: a second copy re-produces the same bytes / same end state.
- Milestone: copy a simple tag (no side buffers) from a `.map` into the running
  game and read it back correctly, repeatably.

### Phase 5 — Bitmaps end-to-end
- Implement `copyBitmapData` into the Spark texture buffer.
- Wire the texture mapping/loading detour (§7), discriminating by whether the
  bitmap resolves into a Spark buffer, and force the texture to reload from CPU
  on patch.
- Milestone: **load a bitmap from a `.map` by name and see it in-game**,
  surviving re-injection.

### Phase 6 — Gbxmodels end-to-end
- Implement `copyVertexData` / `copyIndexData` into the Spark vertex/index
  buffers plus geometry fixups.
- Wire the vertex/index buffer *binding* detour (§7).
- Milestone: **load a gbxmodel from a `.map` by name.**

### Phase 7 — Tooling
- Point TagBrowser / TagDissect at a `MapFile` (source-agnostic), so they can
  browse and dissect an arbitrary `.map` in addition to the live map.
- Milestone: dissect a tag from an on-disk `.map` in the devtools.

---

## 6. Schema distribution (open decision)

Options from `todos.md`, with a leaning:

- **Loose JSON in a `/schemas` dir shipped with Spark** — *preferred*. Lets
  modders tweak schemas without rebuilding. Supports a dev-time override env var
  pointing at the source tree.
- Raw strings compiled into the source — rejected unless distribution proves
  painful.
- Compiled resource — more work, loses easy moddability.

Decision needed in Phase 0: directory location, env-var name for the override,
one combined file vs. one file per group, and versioning/compat expectations.

---

## 7. Engine resource detours

To make the engine consume our copied data, specific engine resource paths are
detoured rather than patching map memory. Detours discriminate by **whether the
resource resolves into a Spark buffer** (a range/marker check on the offset the
tag carries), not by any tag-ownership classification:

- **Bitmaps** — detour the texture buffer *mapping/loading* path. Existing hooks
  `TextureCacheStartLoadingBitmap` / `CacheReadFile` (`spark/hook/HookTable.hpp`,
  currently stubbed in `SparkInputMod.cpp`) are the entry points: record the tag
  being loaded in `startLoadingBitmap`, and in `cacheReadFile` redirect the read
  to our Spark buffer when the bitmap resolves into one. On patch, an existing
  mechanism forces the texture to reload from CPU so the new bytes reach the GPU.
- **Gbxmodels** — detour the vertex/index buffer *binding* path (not
  mapping/loading) so bound geometry points at the Spark vertex/index buffers.
  Spark manages these buffers, so a patch is reflected at bind time.

Decisions needed: lifetime/threading of the "current tag" global, and the exact
binding call to detour for geometry.

---

## 8. Open questions / decisions before we start

1. Final `TagSchema` shape (the nits).
2. Final `MapFile` interface + base enum.
3. `SparkPersistedState` shape (just `tagResourcePathBlock` today) and the
   named-shared-memory scheme (§3.4, §3.4a). Correctness points: the section must
   survive the uninject gap (leaked handle or keeper), and `allocateMapMemory`
   (moved strings, new tag paths/data) must be process-lifetime, not DLL-scoped
   — the engine dereferences it while Spark is uninjected.
4. Schema distribution mechanism and override env var (§6).
5. Engine resource-detour design — texture mapping/loading + vertex/index
   binding (§7).
6. Do `RawMapFile` and `RuntimeMapFile` share overlay struct definitions, or
   stay fully separate as today? (Affects how much of `map_file.hpp` is reused.)
7. Destination allocation strategy for copied tags (reuse `allocateTag` /
   `allocateTagData`, or a Spark-managed arena?) — must support the
   patch/reallocate path and be process-lifetime.
8. `(groupId, path)` matching edge cases: path collisions and group aliases
   (e.g. `jpt!` is both Joint and Damage in `tag.hpp`), and how far dependency
   recursion goes.
9. Resource-string relocation preconditions — `hasRoomForTag` assumes the string
   block starts at `tag[0]`'s path and is contiguous/ascending
   (`verifyResourceStringMonotonicity`). Decide behavior when that fails
   (refuse? fall back?).
10. Dependency staleness (§3.3) — whether/when to switch referenced-tag copy from
    "if absent" to always re-copying the declared closure.

---

## 9. Risks

- **Schema accuracy** — incorrect sizes/offsets silently corrupt copied data.
  Mitigated by `claimBytes` coverage checks and round-trip tests.
- **Side-buffer layout divergence** — raw vs. runtime disagree on vertex/index/
  texture placement; the base model must capture this precisely.
- **Fixup completeness** — missing a handle/reference fixup produces a tag that
  reads fine but crashes on use. Needs targeted per-type tests.
- **String-relocation idempotency** — a re-run of `moveResourceStrings` without
  the `tagResourcePathBlock` guard allocates a second block, leaks the first, and
  double-repoints every `resourcePathAddress`. The guard + shared-section
  survival are the crux; needs explicit re-inject tests.
- **`allocateMapMemory` lifetime** — moved strings, new tag paths, and new tag
  data are dereferenced by the engine *while Spark is uninjected*, so the
  allocator must be process-lifetime. If it were DLL-scoped, uninject would turn
  "ugly but safe" into a crash.
- **Resource-string layout assumption** — the room/relocation math assumes the
  string block is contiguous and ascending from `tag[0]`; a non-conforming map
  silently miscomputes room (see §8 #9).
- **Dependency staleness** — session-scoped buffers + "copy referenced tags only
  if absent" leave a previously-created dependency pointing at this session's
  empty buffer on re-inject (§3.3). Bounded while target closures are shallow;
  fix is to re-copy the declared closure.
- **Stale pointers into buffers** — append can move the backing store, and buffer
  *contents* must never embed pointers into the DLL image (ASLR rebases it).
  Detours must re-resolve offset→pointer at use; fixups must emit only offsets or
  engine/map addresses. Appends may also race reads (loader vs. render thread) —
  append only at safe points or synchronize.
- **Detour scope creep** — the texture and geometry detours must redirect *only*
  resources that resolve into a Spark buffer; misclassification corrupts stock
  rendering.
- **Type churn** — building on `TagSchema`/`MapFile` before the nits are
  settled would cause rework; Phase 0 gates this deliberately.
- **Buffer orphaning (footnote, not a design constraint)** — append-only buffers
  mean each dev-time edit + re-inject appends a fresh copy and orphans the old
  bytes, growing the buffer per iteration until process exit. Acceptable for now;
  an additive compaction/reset can be added if it ever bites.

---

## 10. Out of scope (for now)

- Writing modified tags back out to a `.map` on disk.
- Copy *implemented on* `RawMapFile` — copy runs on `RuntimeMapFile` only; the
  raw map is a read-only source.
- Non-bitmap/non-model tag types beyond what bitmap/gbxmodel transitively need.
- A general tag *editor* in the devtools (browsing/dissecting only).
- Tearing down copied tags/buffers on uninjection (they are left in place).
