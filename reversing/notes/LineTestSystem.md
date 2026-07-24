# Line Test System

Analysis of Halo MCC (H1) collision line/ray testing against entity models. Items marked **[UNVERIFIED]** are hypotheses for dynamic testing.

---

## Overview

The line test system casts a parametric line segment (origin + direction) against different collision geometries. Handlers appear to be dispatched by collision query type — the function `_lineTestVsEntityModel` only activates for type `3` (entity model). Other type values presumably route to separate handlers (BSP world geometry, sphere approximation, etc.).

The function is stored in a function pointer table (sole Ghidra reference is a `.pdata` exception record — no direct call site identified). Dynamic investigation is needed to find the dispatcher and the full dispatch table.

---

## Call Chain

```
[unknown dispatcher — function pointer table, type 3 entry]
  └─ _lineTestVsEntityModel @ B91C0C
       └─ getEntityCollisionData        ← fetches EntityCollisionData for the entity
       └─ _lineTestVsEntityCollisionRegions @ C47940
            └─ invertTransform          ← bone-space inverse
            └─ transformPoint           ← ray origin into bone space
            └─ transformVec             ← ray dir into bone space
            └─ _lineTestVsBSP @ C746E8  ← BSP traversal wrapper
                 └─ FUN_7ff9d8064778    ← actual recursive BSP line test
       └─ transformVec4AsPlane          ← transforms hit plane back to world space
```

---

## `_lineTestVsEntityModel` — Handler for Query Type 3

**Offset:** `0xB91C0C`  
**Confidence:** Medium-high

### Parameters

| Param | Type | Description |
|-------|------|-------------|
| `param_1` | `CollisionQueryObject*` (short*) | Input query. `[0]` = type (must be 3). `[0x38]` = EntityHandle to test against. |
| `param_2` | `Vec3*` | Ray origin |
| `param_3` | `Vec3*` | Ray direction (not normalized; magnitude = segment length) |
| `param_4` | `CollisionResult*` (undefined2*) | Output result structure, initialized to "miss" on entry |

**Returns:** `bool` — `true` if a hit was recorded.

### Logic Summary

1. Validates `*param_1 == 3`. No-ops for any other query type.
2. Computes `end = origin + dir` and `negDir = -dir` (XOR with float negate mask).
3. Calls `getEntityCollisionData()` to fetch the entity's collision data (bone transforms, BSP pointer, etc.). Exits if the entity has no collision data.
4. Calls `_lineTestVsEntityCollisionRegions()` which iterates bone regions, transforms the ray into each bone's local space, and runs the BSP intersection test.
5. On a hit, calls `transformVec4AsPlane()` to bring the hit plane normal from bone-space into world-space. **Confirmed** via standalone `smc64-dlltest` harness (calls the real compiled function with synthetic `Transform`/`Vec4` inputs, offsets `0xBA3090` / `0xBA3004` for `rotateVec`): the transform's `m.x`/`m.y`/`m.z` are basis *column* vectors, `out = m.x*in.x + m.y*in.y + m.z*in.z` (not row-major). Plane transform is `outNormal = rotateVec(m, inNormal)`, `outW = dot(t, outNormal) + inW * s` — standard plane-under-rigid-transform formula. See `smc64-dlltest/src/tests/TransformTests.cpp`.
6. If the hit plane faces away from the ray (`local_46c < 0`), flips all four plane components (normal + distance).
7. Looks up the surface material: `materialTable + 0x24 + surfaceIndex * 0x48`. The `materialTable` pointer is relocated via `DAT_MapBase`/`DAT_RelocatedMapBase`.
8. Fills `param_4` (see layout below) and returns `true`.

---

## CollisionResult Output Layout (`param_4`)

**Confirmed** (2026-07-19) by dynamically calling the real, decompiled `_lineTestVsEntityModel` inside a Unicorn emulator loaded from a captured process dump — see "Dynamic Validation" below. `resultType`, `fraction`, hit point, plane, `materialType`, `entityHandle`, `boneIndex`, and `surfaceIndex` are all confirmed plausible/self-consistent from a real call. The fields at `+0x3c`/`+0x40`/`+0x44`/`+0x48` came back as `0` in that test, which is consistent with (but doesn't prove) the layout below — still **[UNVERIFIED]** for their exact semantics.

| Offset | Type | Content |
|--------|------|---------|
| `+0x00` | `int16_t` | Result type: `0xFFFF` = miss, `3` = entity model hit |
| `+0x14` | `float` | Parametric fraction along ray (0 = origin, 1 = tip) |
| `+0x18` | `float` | Hit point X |
| `+0x1c` | `float` | Hit point Y |
| `+0x20` | `float` | Hit point Z |
| `+0x24` | `Vec4` | Hit plane normal + distance (world space, 16 bytes) |
| `+0x34` | `int16_t` | Surface material type (from collision tag's surface table) |
| `+0x38` | `uint32_t` | Entity handle (copied from query) |
| `+0x3c` | `int16_t` | Permutation selector index — raw `node+0x20` value from `_lineTestVsEntityCollisionRegions` output[1] (index into `Entity+0x13c`'s per-instance byte array, **before** clamping). See `_lineTestVsEntityCollisionRegions` section below. |
| `+0x3e` | `int16_t` | Bone/region index |
| `+0x40` | `int16_t` | Clamped permutation index actually tested this hit (`_lineTestVsEntityCollisionRegions` output[2] = `_clampedPermutationIndex`). See `_lineTestVsEntityCollisionRegions` section below. |
| `+0x44` | `uint32_t` | ? **[UNVERIFIED semantics]** |
| `+0x48` | `int32_t` | `local_46c` — sign used for plane flip; semantics unknown **[UNVERIFIED semantics]** |
| `+0x4c` | `uint8_t` | Flags byte |
| `+0x4d` | `uint8_t` | Flags byte |
| `+0x4e` | `int16_t` | Surface index within BSP surface list |

### Dynamic Validation

Called the real `_lineTestVsEntityModel` (offset `0xB91C0C`) via `smc64-dlltest-unicorn`'s `LineTestVsEntityModelTest.cpp`, using the standard Win64 calling convention (`Win64Call`) against a Unicorn engine loaded from a real captured process dump (`installDemandPaging()`, not just `loadModule()` — this call chain reads entity/tag/BSP data outside the module's own image while executing real code, the first test in the suite to do so).

Target: the player's own cyborg biped (handle `0xE64503D6`, recorded position `(23.066, -17.000, 89.216)` in `approved/EntityList.approved.txt`). Ray: vertical, from 3 units above the recorded position to 3 units below.

Result: a clean hit —
- `resultType = 3`, `fraction ≈ 0.473`
- hit point `(23.066, -17.000, 89.377)` — matches the recorded entity position almost exactly (the small +0.16 Z offset is consistent with the position being close to, but not exactly at, the ground-contact point)
- plane `(≈0, ≈0, -1, -89.377)` — a horizontal ground-ish plane; `dot(normal, hitPoint) == w` checks out (`-1 * 89.377 ≈ -89.377`), confirming the plane storage convention
- `materialType = 21`, `entityHandle` round-tripped correctly (`0xE64503D6`), `boneIndex = 0`, `surfaceIndex = 1` — all in plausible ranges
- the four still-unverified fields (`+0x3c`, `+0x40`, `+0x44`, `+0x48`) all read back as `0` for this particular hit

This also confirms the entire call chain (`getEntityCollisionData` → `getTagDataPointer`/`getBoneTransforms` → `_lineTestVsEntityCollisionRegions` → `invertTransform`/`transformPoint`/`transformVec` → `_lineTestVsBSP` → `FUN_7ff9d8064778` recursive BSP walk → `FUN_7ff9d8064b18`/`FUN_7ff9d810d22c`/`FUN_7ff9d8064d78` leaf/portal tests → `transformVec4AsPlane`) is self-contained within halo1.dll: every function in it touches only the entity pool/record pool, tag array, and `DAT_MapBase`/`DAT_RelocatedMapBase`-relative map data (the same globals `EntityListDumpTest.cpp` already reads), plus a handful of module-local constants (`DAT_unitScale`, `DAT_floatNegateMask`, the security cookie). No calls outside the module, no TLS/`gs:`-segment access, no locks — confirmed by decompiling every function in the chain before running this test.

---

## CollisionQueryObject Layout (`param_1`)

Partial — only fields accessed by `_lineTestVsEntityModel`.

| Offset | Type | Content |
|--------|------|---------|
| `+0x00` | `int16_t` | Query type (3 = entity model) |
| `+0x38` | `EntityHandle` (uint32_t) | Entity to test against |

---

## `_lineTestVsEntityCollisionRegions` — Per-Node Permutation/BSP Selection

**Offset:** `0xC47940`  
**Confidence:** Medium-high (hand-read from a fresh Ghidra decompile 2026-07-23; not yet dynamically isolated on its own — only as part of the whole-chain test in "Dynamic Validation" above)

Iterates `node` over `CollisionTagData->collisionNodes` (`Engine::CollisionNode`, stride `0x40`, count at `CollisionTagData+0x28c`, matching `collision.hpp`'s `CollisionTagData`/`CollisionNode`). For each node (loop variable named `_nodeIndex` in Ghidra):

1. **Permutation selector index** — reads `int16_t` at `node+0x20`. If it's `-1`, the node is skipped entirely (no active geometry for it this frame). Otherwise this value is used as an index into the entity's own per-instance byte array (see below).
2. **Skip empty nodes** — if `node->collisionBsps.count` (at `node+0x34`) is `0`, skip.
3. **Resolve + clamp the active permutation** — reads `collisionData->field6_0x10[selectorIndex]` (a `uint8_t`; `field6_0x10` is `EntityCollisionData+0x10`, which `getEntityCollisionData` points at **`Entity+0x13c`** — Ghidra's own auto-generated name for that field is `field_0x13c_collisionRelated`). This byte is clamped to `[0, node->collisionBsps.count - 1]` to get the final permutation index (`_clampedPermutationIndex`).
4. **Pick the one active `CollisionBSP`** — indexes `node->collisionBsps` (base at `node+0x38`, relocated) by `_clampedPermutationIndex * sizeof(CollisionBSP)` (`0x60` bytes — `CollisionBSP` has 8 `BlockPointer`s, and `engine/tag.hpp`'s `BlockPointer` is 12 bytes: `count`+`offset`+`bullshit`, so `8 * 12 = 0x60`, self-consistent with `bsp.hpp`). **Only this single BSP is tested** — sibling permutations at other indices in the same node are never touched this call.
5. If the selected BSP has geometry (`selectedBsp->bsp3DNodes.count > 0`, checked via the first `int` of the struct), inverts the bone's transform (or uses identity if the bone's scale matches a sentinel value `DAT_7ff9d8c73898`), transforms the ray into bone-local space, and calls `_lineTestVsBSP`.
6. On a hit, writes `[nodeIndex, selectorIndex, clampedPermutationIndex]` back to the output short array (`param_5`, aka `outRegionHit`/`RegionHitOutput` in the hand-decompiled `_lineTestVsEntityModel.hpp`).

**This is the mechanism that decides which single BSP/permutation is "the" collision geometry for a node** — it is **not** "merge every BSP the node has"; it's "the entity has one active permutation per permutation-group (indexed via `node+0x20` into `Entity+0x13c`), clamped to that node's own BSP count, and only that one is live." A node with multiple `collisionBsps` entries is analogous to a render model's Region → Permutations (e.g. damage states, alternate variants) — normally exactly one is "on" at a time, selected by the entity's own state, not something a consumer should merge together.

**Practical implication:** `Mod::Mario::DynamicGeometry.cpp`'s `convertCollisionBSPToSM64Surfaces` currently loops over *every* index in `node->collisionBsps` and merges all their surfaces — this is why some entities were observed rendering overlapping/duplicate collision meshes (every permutation variant stacked together instead of just the active one). The two other existing consumers of this data, `getObjectCollisionBSP` ([`collision.cpp`](../../spark/src/engine/tags/collision.cpp)) and `drawEntityCollision` ([`DrawEntityCollision.cpp`](../../spark/src/mods/devtools/DrawEntityCollision.cpp)), both sidestep this entirely by hard-coding index `0` rather than resolving the real active permutation — that's a reasonable approximation (index 0 is very often the only/default permutation) but not equivalent to what the engine's own line-test actually does.

**Struct-layout implication — `Engine::CollisionNode` in `collision.hpp` needs a second look:** the `char name[0x2C]` guess is very likely wrong for the `+0x20..+0x2C` byte range, since this decompile reads a live binary `int16_t` selector at `node+0x20` — that's *inside* the guessed name field. Halo tag "name" fields are conventionally 32 bytes (`0x20`), which would put a real field boundary exactly at `+0x20` — consistent with the selector living immediately after a `name[0x20]` rather than in the middle of a 44-byte one. The `region`/`parentNode`/`nextSiblingNode`/`firstChildNode` shorts currently guessed at `+0x2C..+0x34` are therefore also suspect (real fields, but possibly at different offsets, or different fields altogether) — **not re-verified here**. Only `+0x20` (permutation selector) and `+0x34`/`+0x38` (`collisionBsps` BlockPointer) are confirmed live reads from this function. `+0x38` is unchanged/still correct either way, since it's independently confirmed by the `collisionBsps.offset` static_assert.

The full output array (`param_5`/`outRegionHit`, 6 shorts) layout (matches `RegionHitOutput` in `_lineTestVsEntityModel.hpp`):

| Index | Content |
|-------|---------|
| `[0]` | Bone/node index of closest hit |
| `[1]` | Permutation selector index (the raw `node+0x20` value, **not** the clamped index) |
| `[2]` | Clamped permutation index actually tested (`_clampedPermutationIndex`) |
| `[4]` | Best `t` (float, reinterpreted as short pair) |
| `[5]` | Init to `0x7f7f` (large float = "no hit yet") |

---

## `_lineTestVsBSP` — BSP Traversal Wrapper

**Offset:** `0xC746E8`  
**Confidence:** Medium

Thin wrapper that clamps `t_max` to `[0, DAT_7ff9d8bf0ef8]`, packages parameters, and calls `FUN_7ff9d8064778` — the actual recursive BSP line test.

The `t` parameter convention used internally is **inverse-parametric**: `t=0` is at the end of the segment, `t=1` is at the origin. `_lineTestVsEntityModel` converts: `fraction = 1.0 - t`.

---

## Open Questions / Dynamic Experiments Needed

1. **What is the dispatcher?** Find the function pointer table containing `_lineTestVsEntityModel` and identify all sibling type handlers (types 0, 1, 2 at minimum). Set a breakpoint at the table read or at the start of `_lineTestVsEntityModel` to catch callers in a live session.
2. ~~**Confirm CollisionResult offsets**~~ — **Done.** Confirmed dynamically via `smc64-dlltest-unicorn`'s `LineTestVsEntityModelTest.cpp` (see "Dynamic Validation" above) by calling the real function against a captured dump rather than a live-session breakpoint. `resultType`, `fraction`, hit point, plane, `materialType`, `entityHandle`, `boneIndex`, and `surfaceIndex` are all confirmed.
3. **What is `local_46c`** (the sign-flip field at `+0x48`)? Read back as `0` in the one dynamic test run so far — possibly a "back-face" indicator or a surface normal orientation flag that's only nonzero for certain hit types. Try a ray that hits a back-facing surface (e.g. from inside a BSP volume looking out) to get a nonzero sample.
4. **Surface material stride** — the `0x48` stride matches Halo's `collision_model` surface struct. Confirm against HCEEK tag definitions. (`materialType = 21` was observed for the player biped hit in the dynamic test — cross-reference against the biped's collision tag surfaces in HCEEK to confirm the meaning of type 21.)
5. ~~**Fields `+0x3c`/`+0x40`**~~ — **Mostly resolved (2026-07-23)**, from a fresh Ghidra decompile of `_lineTestVsEntityCollisionRegions` (see its section above): `+0x3c` is the raw (unclamped) permutation-selector index read from `node+0x20`, and `+0x40` is the clamped permutation index actually tested. Still not dynamically re-validated in isolation (both read back as `0` in the one biped test, consistent with a single-permutation node). `+0x44` remains unexplained.
6. ~~**`Engine::CollisionNode`'s layout in `collision.hpp` needs re-verification**~~ — **Hypothesis corroborated (2026-07-23):** the three-part chain (`Entity+0x13c` = start of a per-instance active-permutation byte array → `EntityCollisionData+0x10` is a live pointer into it → `CollisionNode+0x20` is the selector index into it) is now cross-referenced across `engine/entity/types.hpp`, `EntityCollisionData.hpp`, and `collision.hpp`. Still not dynamically isolated (no confirmed byte values read from a real multi-permutation entity yet), and the exact byte boundaries of `CollisionNode` between `+0x20` and `+0x34` (and the true length of `Entity`'s permutation array) remain unconfirmed. Next step would be a dynamic dump of a real multi-permutation entity (e.g. a vehicle with a damage-state part) to read nonzero values out of `Entity+0x13c` and confirm the array's extent.
