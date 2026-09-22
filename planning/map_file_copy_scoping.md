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

### 3.3 Copy semantics

Copy is **target-driven**: `RuntimeMapFile::copyTag(sourceMap, sourceHandle)`
runs on the destination (the live map) and pulls from `sourceMap`. Only the
runtime implementation copies; `RawMapFile` is read-only source (see §10).

A generic copy should:
- Traverse the source tag via its schema.
- Bulk-copy structures (not field-by-field) where layouts match.
- Apply **fixups** after the bulk copy for anything that can't be copied
  verbatim: relative pointers (block pointers, texture/vertex/index data),
  cross-tag references, handles/IDs that must be re-issued in the destination.
- Translate every relative pointer through the base model in §3.1: compute
  `sourcePtr - sourceBase`, then re-anchor onto `destBase`.
- **Recursively copy referenced tags** that are absent from the target map, and
  **patch tags that are already present** (leave data in place when the size
  matches, reallocate when it differs). Tags are matched between maps by their
  `(groupId, path)` tuple, which also makes the copy idempotent — a re-run
  patches in place instead of duplicating.
- Return a ready-to-use tag handle in the destination.

Bitmap pixel data and model vertex/index data are the notable "side buffers"
that live outside the tag-data blob and need representation-specific handling
(raw map → map/vertex/index-relative offset; runtime → a **Spark-managed
buffer**, §3.3a).

### 3.3a Spark-managed buffers (`SparkBuffers`)

At runtime, copied side-buffer data does not go into the engine's map memory —
it goes into Spark-owned buffers for texture, vertex, and index data. The engine
is then detoured to consume them:

- **Texture** buffer *mapping/loading* is detoured for any Spark-owned tag.
- **Vertex/index** buffer *binding* is detoured (not mapping/loading).
- `copyBitmapData` / `copyVertexData` / `copyIndexData` return offsets into
  these buffers (whereas `RawMapFile` would return map/vertex/index-relative
  offsets).

The buffers are **append-only POD arenas** (`{ data, size, capacity }`), not
live STL containers. Tags are never removed, so copy appends and hands out fixed
offsets. Two consequences fall out of this:

- **Re-resolve offset→pointer at every use.** Offsets are stable across growth;
  raw pointers are not (an append may `realloc`/move `data`). Detours must
  compute `arena.data + offset` at use time, or reserve up front and not grow
  while the engine holds a live pointer (texture streaming is the risk).
- **Alignment.** Byte-granular storage isn't enough for GPU vertex/index binding
  or texture rows; each appended block must be padded and its offset aligned.

`getSparkBuffers()` exposes them. Only a small, fixed-size **`SparkPersistedState`**
(a `SparkBuffers*` plus bookkeeping) lives in named shared memory (§3.4); the
variable-size **buffer content stays in ordinary process memory**, not in the
shared section. The persisted pointer stays valid across re-injection only
because that content is **intentionally never destructed** — with Spark's
`staticruntime "off"`, a deliberately-leaked allocation survives on the shared
CRT heap and can even be grown/freed by the next injection. A static/global with
a destructor would instead be freed at `DLL_PROCESS_DETACH` and dangle the
pointer. `VirtualAlloc` is an equally valid, CRT-independent backing store.
POD arenas (rather than `std::vector`) are deliberate: the persisted object is
reinterpreted by the *next* injected build, so its layout must not depend on STL
ABI — which differs across Debug/Release and toolset updates (§9). Concrete shape
of both structs is TBD.

### 3.4 Inject / re-inject lifecycle & idempotency

This is a first-class requirement, not a nicety. During development the DLL is
repeatedly uninjected and re-injected against a running game, and the pipeline
must tolerate that:

- **Persisted state** — `getPersistedState()` returns a `SparkPersistedState`
  backed by **named shared memory** (small and fixed-size: a `SparkBuffers*`
  plus bookkeeping). It survives uninject/re-inject cycles but *not* process
  exit. Buffer *content* lives in ordinary process memory and must be
  intentionally leaked (not destructed on unload) so the persisted pointer stays
  valid — see §3.3a. This is how Spark reattaches to its buffers on re-injection.
- **Idempotent copy** — re-running `copyTag` for the same source tag patches the
  existing tag rather than duplicating it (in place if same size, reallocate if
  different); `(groupId, path)` matching drives this.
- **New tags left in place at uninject time** — copied tags/data are not torn
  down on uninjection.
- **Graceful while-uninjected behavior** — custom tags point at junk data while
  uninjected; this has been tested to be visually ugly but *not* a crash, so it
  is acceptable between sessions.

---

## 4. Proposed architecture (decoupled)

A single polymorphic `MapFile` role with two implementations, plus a schema
collection accessor. **Names and signatures below are placeholders** pending
the type nits.

- `MapFile` (interface / role):
  - Translate to/from a relative base (the §3.1 base enum).
  - Enumerate/find tags (by name/path, by group).
  - `copyTag(sourceMap, sourceHandle) -> destHandle` — the public entry point,
    implemented on the **target** map; orchestrates defaults, dependency copy,
    data copy, and fixups. Idempotent via `(groupId, path)` matching: absent
    tags are copied, present tags are patched (in place if same size, reallocated
    if different).
  - Internal, representation-specific side-buffer copy: `copyTagData(...)`,
    `copyBitmapData(...)`, `copyVertexData(...)`, `copyIndexData(...)`.

- `RawMapFile : MapFile` — backed by a file buffer (folds in today's
  `Engine::MapFile::MapFile` capabilities). **Read-only source**: offsets are
  map/vertex/index-relative; copy methods are not implemented here.

- `RuntimeMapFile : MapFile` — backed by the live process (folds in today's
  `engine/map.hpp` + `engine/tag.hpp` access). **The only implementation that
  copies.** Side buffers resolve into `SparkBuffers` (§3.3a) rather than
  map-relative offsets.

- `getRuntimeMap()` — returns the cached runtime map, refreshed on scenario
  load.

- `getSparkBuffers()` — returns the Spark-managed texture/vertex/index buffers,
  reattached from `SparkPersistedState` on injection (§3.4).

- `getPersistedState()` — returns the named-shared-memory `SparkPersistedState`
  (holds a pointer to `SparkBuffers`); survives re-injection, not process exit.

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
- Decide the `SparkBuffers` shape and its shared-memory bookkeeping (§3.3a).
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

### Phase 3 — SparkBuffers + lifecycle foundation
- Stand up the Spark-managed texture/vertex/index buffers and the
  `SparkPersistedState` in named shared memory, including reattach on
  (re-)injection.
- Establish the idempotency primitives: `(groupId, path)` lookup in the target
  map, patch-vs-reallocate decision, "is this tag one of ours" flagging.
- Milestone: buffers survive an uninject/re-inject cycle and are reattached
  rather than reallocated.

### Phase 4 — Generic tag-data copy
- Implement schema-driven `copyTagData` on `RuntimeMapFile` (bulk copy +
  block-pointer fixups + base re-anchoring) for `RawMapFile → RuntimeMapFile`.
- Implement recursive dependency copy and present-tag patching (matched by
  `(groupId, path)`) plus a small set of custom field fixups (handles/IDs,
  cross-tag refs).
- Verify idempotency: copying the same tag twice patches in place on the second
  run rather than duplicating.
- Milestone: copy a simple tag (no side buffers) from a `.map` into the running
  game and read it back correctly, repeatably.

### Phase 5 — Bitmaps end-to-end
- Implement `copyBitmapData` into the Spark texture buffer.
- Wire the texture mapping/loading detour (§7) so the engine consumes our
  copied bitmap data for Spark-owned tags.
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

To make the engine consume our copied data for Spark-owned tags, specific engine
resource paths are detoured rather than patching map memory:

- **Bitmaps** — detour the texture buffer *mapping/loading* path. Existing hooks
  `TextureCacheStartLoadingBitmap` / `CacheReadFile` (`spark/hook/HookTable.hpp`,
  currently stubbed in `SparkInputMod.cpp`) are the entry points: record the tag
  being loaded in `startLoadingBitmap`, and in `cacheReadFile` redirect the read
  to our Spark buffer when the in-flight tag is one of ours.
- **Gbxmodels** — detour the vertex/index buffer *binding* path (not
  mapping/loading) so bound geometry points at the Spark vertex/index buffers.

Decisions needed: lifetime/threading of the "current tag" global; how a tag is
flagged as "one of ours" (ties into the `(path, groupId)` bookkeeping); and the
exact binding call to detour for geometry.

---

## 8. Open questions / decisions before we start

1. Final `TagSchema` shape (the nits).
2. Final `MapFile` interface + base enum.
3. `SparkBuffers` / `SparkPersistedState` shape and the named-shared-memory
   reattach scheme (§3.3a, §3.4). Correctness point: buffer content must outlive
   DLL unload — with `staticruntime "off"` a deliberately-leaked heap allocation
   suffices (or `VirtualAlloc`); a destructed static would dangle the pointer.
4. Schema distribution mechanism and override env var (§6).
5. Engine resource-detour design — texture mapping/loading + vertex/index
   binding (§7).
6. Do `RawMapFile` and `RuntimeMapFile` share overlay struct definitions, or
   stay fully separate as today? (Affects how much of `map_file.hpp` is reused.)
7. Destination allocation strategy for copied tags (reuse `allocateTag` /
   `allocateTagData`, or a Spark-managed arena?) — must support the
   patch/reallocate path and not be destructed on DLL unload.
8. `(groupId, path)` matching edge cases: path collisions and group aliases
   (e.g. `jpt!` is both Joint and Damage in `tag.hpp`), and how far dependency
   recursion goes.

---

## 9. Risks

- **Schema accuracy** — incorrect sizes/offsets silently corrupt copied data.
  Mitigated by `claimBytes` coverage checks and round-trip tests.
- **Side-buffer layout divergence** — raw vs. runtime disagree on vertex/index/
  texture placement; the base model must capture this precisely.
- **Fixup completeness** — missing a handle/reference fixup produces a tag that
  reads fine but crashes on use. Needs targeted per-type tests.
- **Lifecycle correctness** — non-idempotent copy or failed buffer re-discovery
  leaks/duplicates data across re-injection, or dangles pointers. The
  shared-memory bookkeeping is the crux and needs explicit re-inject tests.
  Note this quietly depends on `staticruntime "off"`; flipping it to `"on"`
  would turn the persisted pointer into a use-after-free.
- **Persisted-state ABI** — the persisted object is reinterpreted by the *next*
  injected build, so any STL type embedded in it couples all builds to one STL
  ABI (Debug vs. Release `std::vector` layouts already differ). Mitigated by
  keeping `SparkBuffers`/`SparkPersistedState` POD (append-only arenas).
- **Stale pointers into buffers** — arena growth can move the backing store, and
  buffer *contents* must never embed pointers into the DLL image (ASLR rebases
  it). Detours must re-resolve offset→pointer at use; fixups must emit only
  offsets or engine/map addresses. Appends may also race reads (loader vs.
  render thread) — append only at safe points or synchronize.
- **Detour scope creep** — the texture and geometry detours must apply *only* to
  Spark-owned tags; misclassification corrupts stock rendering.
- **Type churn** — building on `TagSchema`/`MapFile` before the nits are
  settled would cause rework; Phase 0 gates this deliberately.

---

## 10. Out of scope (for now)

- Writing modified tags back out to a `.map` on disk.
- Copy *implemented on* `RawMapFile` — copy runs on `RuntimeMapFile` only; the
  raw map is a read-only source.
- Non-bitmap/non-model tag types beyond what bitmap/gbxmodel transitively need.
- A general tag *editor* in the devtools (browsing/dissecting only).
- Tearing down copied tags/buffers on uninjection (they are left in place).
