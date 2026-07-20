# World Bone System

Analysis of how Halo CE computes, propagates, and commits entity bone transforms each frame. Prompted by the [[WeaponPoseBug]].

## Key Functions

| Name | Offset | Confidence |
|---|---|---|
| `updateWorldBones` | `0xB3A614` | High (already named) |
| `propagateWorldBones` | `0xB3A57C` | High |
| `_entityTickUpdate` | `0xB3A06C` | Medium |
| `_updateAttachedEntityBones` | `0xB3AF10` | Medium |
| `_attachEntityToBone` | `0xB375D8` | Medium |
| `_moveEntityAndUpdate` | `0xB359E8` | Medium |
| `getBoneTransforms` | `0xB36E64` | High |
| `_commitEntityRenderState` | `0xBA8414` | Medium |
| `rotateVec` | `0xBA3004` | High (dynamically confirmed) |
| `transformVec` | `0xBA2F5C` | High (dynamically confirmed) |
| `transformPoint` | `0xBA2EA8` | High (dynamically confirmed) |
| `invertTransform` | `0xBA2214` | High (dynamically confirmed) |
| `transformVec4AsPlane` | `0xBA3090` | High (dynamically confirmed) |

### Transform math family — confirmed via `smc64-dlltest`

All five functions above were called directly (standalone, outside MCC) with
synthetic `Transform`/`Vec3`/`Vec4` inputs and their output checked against
hand-derived expected values -- see `smc64-dlltest/src/tests/TransformTests.cpp`.
Key findings:

- `Transform.m`'s `x`/`y`/`z` are basis **column** vectors: `rotateVec` computes
  `out = m.x*in.x + m.y*in.y + m.z*in.z`. Matches the existing
  `Engine::Transform::transformVec()`/`transformPoint()` member methods in
  `engine/math.hpp` for rotation.
- **`rotateVec` ignores scale entirely.** `transformVec`/`transformPoint`,
  by contrast, multiply the input vector by `Transform.w` (scale) *before*
  rotating -- unless `w == 1.0` exactly (`DAT_unitScale`, confirmed by reading
  the constant from the loaded module), in which case the multiply is skipped.
  **This means the existing `Engine::Transform::transformVec()`/
  `transformPoint()` member methods do NOT match the real engine functions
  when scale != 1** (they never apply `w`). Likely low-impact in practice
  since bone scale is normally 1.0 (per the note above on "Scaling bones
  seems pretty rare" in `Reversing.md`), but worth fixing if any codepath
  relies on non-unit bone scale. `_attachEntityToBone` (below) uses
  `rotateVec` (not `transformVec`) for velocity/facing, which is consistent
  with those being pure directions that shouldn't be scaled.
- `invertTransform` is the standard similarity-transform inverse:
  `invScale = 1/scale` (or `1.0` if scale is already `1.0`), rotation matrix
  is transposed, and `invTranslation = R^T * (-translation * invScale)`.
  Confirmed both against a hand-derived example (asymmetric cyclic-permutation
  rotation, to actually exercise the transpose swap) and an invert-twice
  round-trip recovering the original transform exactly.



## `updateWorldBones` — What It Does

This function **fully reconstructs all world-space bone transforms from scratch** on every call. There is no incremental update; the output buffer is always overwritten.

### Two distinct bone buffers

| Buffer | Self-relative ptr offset | Layout | Purpose |
|---|---|---|---|
| **Pose buffer** | `+0x1A8` | 8 floats/bone: `[Quaternion rotation \| {x=tx, y=ty, z=tz, w=bone_scale}]` | Written by animation evaluator and overlays |
| **World bone buffer** | `+0x1AE` (`entity->worldBones` `RelativePointer`) | `Transform` (0x34 bytes/bone: `float scale, Mat3 rotation, Vec3 translation`) | Written by BFS pass; what `worldBones.get()` returns in the mod. Retrieved via `getBoneTransforms(handle)` |

### Phase 1 — No model tag

If `entity.tag (field_0x34) == -1` (no gbxmodel), a trivial single-bone `Transform` is built directly from the entity's `pos`, `forward`, and `up` vectors. Cross product fills the third (right) axis.

### Phase 2 — Has model tag

1. **Animation evaluation** — `FUN_7fff4aa41984(modelTag, animState, frameIndex, poseBuffer)` writes object-space quaternion poses into the **pose buffer**.

2. **Animation overlays** — `FUN_7fff4aa42478` (frame-driven) and `FUN_7fff4aa421a4` (random-seeded cyclic) apply additive overlays. Random seed is `(globalTick + entityHandle) % period`.

3. **Entity-level scale** — if `entity[0x6c] > 0`, the root bone's `{tx, ty, tz, bone_scale}` in the **pose buffer** are multiplied by the scale value before BFS. Uniformly grows/shrinks the model.

4. **Animation graph + blend** — `FUN_7fff38cb5430` (animation graph state machine, confidence: medium) runs if `modelTag[0x44]` is present. `lerpQuaternionTransforms(boneCount, secondaryPose, poseBuffer, animFrame2, animFrameCount)` blends two animation poses in-place into `poseBuffer` when `entity->animFrameCount > 0`. Secondary pose pointer is nullable (blends with default when null). Bogus frame values cause flaking poses at animation transitions.

5. **BFS world-space pass** — iterates bone hierarchy breadth-first (`local_d8[64]` queue, starting at root = bone 0). For each bone:
   - `setTransformFromQuaternion(poseBuffer[i].rotation)` → local `Mat3`
   - Unpack translation from `poseBuffer[i+1].{x,y,z}`
   - **Root bone, not attached**: construct parent transform from entity pos/facing/up (see below)
   - **Root bone, attached** (`entity->parentAttachHandle` != -1): parent = `getBoneTransforms(parent)[boneIndex]`, re-fetched live each call
   - **Non-root**: parent = `worldBones[parentBoneIndex]` (already written by BFS)
   - `multiplyTransforms(parent, localBone)` → `worldBones[i]`
   - Enqueue `firstChild` and `secondChild` bone indices from bone hierarchy table

   **Root parent transform construction (not attached):**
   1. `setTransformFromForwardUp(entity.forward, entity.up)` → `local_980`
   2. Apply optional model attachment-point offset (`multiplyTransforms`)
   3. Apply model tag placement offset (`multiplyTransforms`)
   4. Build `local_9f0` = entity-position transform (entity.pos + identity-ish mat)
   5. `multiplyTransforms(local_9f0, local_980)` → root parent transform

6. **Commit pivot** — `transformPoint(worldBones[0], modelTag.originOffset, &entity.renderPos)` transforms the model's tagged origin through the root world bone and stores the result as the entity's render pivot.

7. **Render frame tag** — model tag handle written to `entity[0x68]`.

### Attachment branch

When `entity[0x10c]` (parentAttachHandle) != -1, the root bone parent transform is the parent entity's attachment bone world `Transform`, fetched live from `getAttachmentTransformTable`. The child's animation is still evaluated in pose-buffer-local space and stacked on top. This is how a weapon tracks a grip bone on the player each frame.

### Entity type gating

`entity[0xa4].type` in range `[5, 11]` → pose buffer alias points to a **local stack buffer**. Bone writes are discarded when the function returns. These types have no persistent world bones.

---

## Calling Contexts

### Per-frame entity tick (`_entityTickUpdate`)

Called for every active entity each frame. Inside, `updateWorldBones` is called **unless `entity.flags[0x44] & 0x800000` is set**.

After `updateWorldBones`, it recurses:
- → `entity[0x108]` (firstChildHandle) — always
- → `entity[0x104]` (nextSiblingHandle) — **only if this entity is itself attached** (`entity[0x10c]` != -1)

**Attached entity early-exit**: if `DATA_ENTITY_LIST[idx].field1_0x2 & 0x10`, the function calls `_updateAttachedEntityBones` instead and **returns early without calling `updateWorldBones`**. Bones for attached entities are managed by `propagateWorldBones`.

### `propagateWorldBones`

Calls `updateWorldBones` on `entity`, then walks the `firstChild → nextSibling` chain and recursively calls itself for each child.

Triggered whenever an entity's position is changed externally (physics step, teleport, script move). Ensures all attached children recompute their world bones using the parent's **already-updated** attachment bone.

Because this calls `updateWorldBones` directly, **the `UpdateWorldBones` mod hook fires for every entity in the subtree**, including the weapon. Custom bone writes applied in the hook will be re-applied.

### `_attachEntityToBone`

Called when `objects_attach` runs. Converts child's world-space state into parent-bone-local space (see below), then sets up `entity[0x10C]` (parent handle), `entity[0x110]` (bone index), sets bit 0x10 on the entity list record, and calls `updateWorldBones(child)` once to produce initial world bones.

### `_moveEntityAndUpdate`

Called when an attached entity needs its position updated (e.g. parent moved). Computes the new child position from parent attachment bone, sets entity pos/vel, then calls `_moveEntityAndUpdate` → `updateWorldBones`.

---

## The Render Commit Step (`_commitEntityRenderState`)

This is **separate** from `updateWorldBones`. It copies the finalized world bones and entity position into a per-entity render buffer (stride `0xd4c` bytes, indexed by entity index). The renderer reads from this buffer.

For **attached entities** (bit 0x10), `_updateAttachedEntityBones` calls `_commitEntityRenderState` directly, reading whatever is currently in the world bone buffer.

For **normal entities**, `_entityTickUpdate` ends by calling `FUN_7fff4aa702a0` which handles the render commit path.

**Implication**: world bones must be finalized before `_commitEntityRenderState` runs. This is why writing bones during `UpdateAllEntities` (before the render commit) is insufficient — `updateWorldBones` will overwrite them later in the same tick.

---

## Weapon Overwrite Bug Analysis

See [[WeaponPoseBug]] for the observed symptoms.

### What is confirmed

1. **`updateWorldBones` always overwrites bones from animation state.** The mod hook correctly applies custom weapon pose *after* `next()`, so normal-path calls are handled.

2. **The `propagateWorldBones` path also goes through the mod hook.** Any call to `updateWorldBones` — including those from `propagateWorldBones` — fires the `UpdateWorldBones` hook. So propagation alone should not cause the bug.

3. **The `0x800000` flag** in `entity.flags[0x44]` skips `updateWorldBones` in `_entityTickUpdate`. Setting this flag on the weapon would prevent the engine from calling `updateWorldBones` via the normal tick path, but `propagateWorldBones` calls it unconditionally regardless of this flag.

4. **Bit 0x10 on the entity list record** (`DATA_ENTITY_LIST[idx].field1_0x2`) marks an entity as "attached." When set, `_entityTickUpdate` skips `updateWorldBones` entirely for that entity. The bones are only ever updated when `propagateWorldBones` runs on the parent, or when `_attachEntityToBone` sets up the initial position.

### Open question

The exact mechanism by which attaching a **projectile** causes the weapon pose to be frozen or overwritten is not yet confirmed. The "freezing the projectile also freezes the weapon" observation strongly implies the weapon's bones are being derived as a side effect of the projectile's update. 

Leading hypotheses:
- The projectile's attachment to the player triggers `propagateWorldBones(player)` via some path not yet traced (possibly inside `objects_attach` scripting dispatch or a higher-level wrapper around `_attachEntityToBone`). This propagation would call `updateWorldBones(weapon)` **outside the normal per-entity tick order**, at a point where the mod has already finished its hook pass for that frame.
- Alternatively, `_entityTickUpdate`'s recursion order may deliver `updateWorldBones(weapon)` after the mod's hook pass completes for some frames when the projectile is present, due to the child/sibling traversal order changing.

### Potential fix directions

1. **Flag 0x800000**: Set `entity.flags & 0x800000` on the weapon to prevent the per-tick `updateWorldBones` call. The mod hook still fires via `propagateWorldBones` since that path ignores the flag. Needs verification that render commit still happens.

2. **Bit 0x10 approach**: Setting bit 0x10 on the weapon's entity list entry would completely prevent `_entityTickUpdate` from touching it. Bones would only be updated when `propagateWorldBones` runs — which the mod hook still intercepts. Risk: side effects of the `_updateAttachedEntityBones` path (calls `_commitEntityRenderState` directly, skips other entity tick steps).

3. **Investigate `objects_attach` dispatch**: Decompile `FUN_7fff4a9c7158` (scripting handler) and check whether it calls `propagateWorldBones(player)` after `_attachEntityToBone`. If yes, that's the overwrite trigger and the fix is to ensure the mod's weapon pose re-apply happens after that call completes.

---

## `_attachEntityToBone` — Detailed Steps

1. **Cycle check** — walks ancestor chain via `entity[0x10C]` to confirm child is not already an ancestor of parent. Returns early if cycle detected.
2. Fetches `getBoneTransforms(parentHandle)[boneIndex]` → `parentBoneWorld`.
3. **Zero-scale safety**: if `parentBoneWorld.scale == 0`, skips invert and zeroes local transform.
4. `invertTransform(parentBoneWorld, &inverted)`
5. Converts child world-space state into parent-bone-local space:
   - `transformPoint(&inverted, child.pos, child.pos)`
   - `rotateVec(&inverted, child.vel, child.vel)`
   - `rotateVec(&inverted, child.facing, child.facing)`
6. Sets `entity[0x10C] = parentHandle`, `entity[0x110] = boneIndex`
7. Sets `DATA_ENTITY_LIST[child.index].field1_0x2 |= 0x10`
8. `updateWorldBones(child)` — initial world bones from now-local offsets

On each subsequent `updateWorldBones` call, the parent bone's current world `Transform` is re-fetched live from the attachment transform table and composed with the stored local offset, so the child tracks the parent bone automatically.

---

## Entity Attachment Field Map

Offsets are from the entity data base (`entity_ptr + 0x34` in raw memory, i.e. the start of the entity-specific fields).

| Offset | Field | Notes |
|---|---|---|
| `+0x104` | `nextSiblingHandle` | Next sibling in parent's attachment list |
| `+0x108` | `firstChildHandle` | Head of this entity's attachment list |
| `+0x10C` | `parentAttachHandle` | Parent entity handle (if attached) |
| `+0x110` | `parentBoneIndex` (byte) | Which bone on parent this is attached to |
| `+0x1AA` | `RelativePointer` → pose buffer | Animation quaternion poses; 8 floats/bone. `uint16` offset, `0xFFFF` = null |
| `+0x1AE` | `entity->worldBones` (`RelativePointer`) → world bone buffer | `Transform[]`; 0x34 bytes/bone. Resolved as `entity_base + (short)offset`. Read by attached children via `getBoneTransforms` |
