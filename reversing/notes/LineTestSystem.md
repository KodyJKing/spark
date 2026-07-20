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
5. On a hit, calls `transformVec4AsPlane()` to bring the hit plane normal from bone-space into world-space.
6. If the hit plane faces away from the ray (`local_46c < 0`), flips all four plane components (normal + distance).
7. Looks up the surface material: `materialTable + 0x24 + surfaceIndex * 0x48`. The `materialTable` pointer is relocated via `DAT_MapBase`/`DAT_RelocatedMapBase`.
8. Fills `param_4` (see layout below) and returns `true`.

---

## CollisionResult Output Layout (`param_4`)

**[UNVERIFIED — offsets derived from decompiler, not dynamically confirmed]**

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
| `+0x3c` | `int16_t` | ? (from `_lineTestVsEntityCollisionRegions` output[1]) |
| `+0x3e` | `int16_t` | Bone/region index |
| `+0x40` | `int16_t` | ? (from `_lineTestVsEntityCollisionRegions` output[3]) |
| `+0x44` | `uint32_t` | ? |
| `+0x48` | `int32_t` | `local_46c` — sign used for plane flip; semantics unknown **[UNVERIFIED]** |
| `+0x4c` | `uint8_t` | Flags byte |
| `+0x4d` | `uint8_t` | Flags byte |
| `+0x4e` | `int16_t` | Surface index within BSP surface list |

---

## CollisionQueryObject Layout (`param_1`)

Partial — only fields accessed by `_lineTestVsEntityModel`.

| Offset | Type | Content |
|--------|------|---------|
| `+0x00` | `int16_t` | Query type (3 = entity model) |
| `+0x38` | `EntityHandle` (uint32_t) | Entity to test against |

---

## `_lineTestVsEntityCollisionRegions` — Per-Region BSP Dispatcher

**Offset:** `0xC47940`  
**Confidence:** Medium

Iterates over an entity's collision regions (count at `EntityCollisionData+0x8+0x28c`). For each region:

1. Looks up the region's bone permutation via a byte-indexed clamped table — determines which permutation is active based on the entity's current bone data.
2. If the permutation's BSP has faces (`piVar8[0] > 0`), proceeds.
3. Inverts the bone's transform (or uses identity if the scale matches a sentinel value).
4. Transforms the ray origin and direction into bone-local space.
5. Calls `_lineTestVsBSP` with clamped entry `t` (from the previous best hit, or 0).
6. On a hit, writes `[bone, permutation, region]` back to the output short array (`param_5`).

The output array (`param_5`, 6 shorts) layout:

| Index | Content |
|-------|---------|
| `[0]` | Bone index of closest hit |
| `[1]` | Permutation index |
| `[2]` | Sub-region index |
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
2. **Confirm CollisionResult offsets** by breaking on `_lineTestVsEntityModel` return with `AL=1` and inspecting `param_4` in memory.
3. **What is `local_46c`** (the sign-flip field at `+0x48`)? Possibly a "back-face" indicator or a surface normal orientation flag.
4. **Surface material stride** — the `0x48` stride matches Halo's `collision_model` surface struct. Confirm against HCEEK tag definitions.
