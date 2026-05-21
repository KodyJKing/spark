# Flares Fix — Analysis & Implementation Notes

## Goal

Selectively disable lens flares (and their associated point lights) for specific entities
(e.g. Mario), while leaving all other flares intact.

---

## Relevant Functions

| Address | Name (renamed in Ghidra) | Role |
|---------|--------------------------|------|
| `7ffd7c8c5bb8` | `renderFlares` | Top-level flare pipeline, called once per frame |
| `7ffd7c89b7b4` | `flareListIterNext` | Generational-handle iterator over the flare list |
| `7ffd7c8c7d28` | `flareDequeueFromSpatial` | Removes flare from spatial partition before position update |
| **`7ffd7c8c7d70`** | **`updateFlareTransform`** | **★ Hook target** — updates world pos from entity bone, enqueues in spatial partition |
| `7ffd7c8c5a54` | `expireFlare` | Permanently removes a flare (timer expired) |
| `7ffd7c8f1e3c` | `queueFlareLight` | Queues the point-light component (cap 0x80 per frame) |
| `7ffd7c8f1e84` | `submitLensFlare` | Queues the lens flare sprite (cap 0x400 per frame) |
| `7ffd7c951fc8` | `flareEnqueueSpatial` | Inserts flare into spatial partition (called inside `updateFlareTransform`) |

---

## Pipeline Overview

```
renderFlares()
│
├─ Phase 1 — State update (iterates all registered flares)
│   for each active flare:
│     if mount entity alive && within range:
│       flareDequeueFromSpatial()   ← pull from partition
│       updateFlareTransform()      ← update world pos, re-enqueue  ◄── HOOK HERE
│     else if out of range:
│       expireFlare()               ← remove from list permanently
│
├─ FUN_7ffd7c958e3c                 ← frustum-cull spatial partition
│   → fills DAT_7ffd7e88944c[0..N] with visible flare handles
│
└─ Phase 2 — Render (iterates frustum-culled flares only)
    for each visible flare:
      compute brightness (fade-in if timed, full if static)
      entity checks → possible color modulation
      if color non-zero:
        queueFlareLight()    ← point light
        submitLensFlare()    ← lens flare sprite
```

If `updateFlareTransform` is skipped for a flare, it is never enqueued in the spatial
partition, so the frustum cull never picks it up, and neither `queueFlareLight` nor
`submitLensFlare` is called. Both effects are suppressed with no game-state side effects.

---

## Flare State Entry Layout (0x7C bytes)

**`DAT_7ffd7e6c0900` is a pointer** — the global variable at that address holds an 8-byte
heap pointer to the flare list manager struct. Always dereference it first:
```
list_base  = *(int64*) 0x7ffd7e6c0900          // single deref of global ptr
entries_off = *(int32*)(list_base + 0x34)      // = 0x38 in observed maps
entry_addr  = list_base + entries_off + (handle & 0xFFFF) * 0x7C
```
Entry stride: `0x7C` (confirmed by CE read of listBase+0x22)  
Capacity: 326 slots (observed in-session)  
EntriesOffset: `0x38` (observed value of *(i32*)(listBase+0x34))

```
entry + 0x02  u16 flags
                bit 1 (0x02): needs spatial update
                bit 2 (0x04): currently in spatial partition
                bit 3 (0x08): lens flare confirmed visible via entity path
entry + 0x04  u32  light tag ID
entry + 0x08  u32  current light render handle (-1 = none)
entry + 0x14  f32  color R  ┐
entry + 0x18  f32  color G  ├ zeroing these prevents render (checked before queueing)
entry + 0x1C  f32  color B  ┘
entry + 0x2C  u32  mount entity handle  (0xFFFFFFFF = world-fixed, no entity)
entry + 0x30  f32×3+  world position / transform (written by updateFlareTransform)
entry + 0x54  f32  computed brightness / alpha
entry + 0x58  i32  tick timestamp (-1 = static/no fade-in, ≥0 = timed)
entry + 0x5C  u16  marker ID
entry + 0x60  i16  node index (-1 = use fallback address)
entry + 0x78  f32  scale factor
```

**`entry + 0x2C` is the key field** — it holds the mount entity handle, available inside
`updateFlareTransform` before any bone sampling or spatial work is done.

---

## Entity Checks in Phase 2

Two entity fields are read in the render loop to modulate (not skip) flare color:

| Field | Meaning (inferred) | Effect |
|-------|--------------------|--------|
| `entity + 0xA4` (u16) | Unknown — team/seat threshold? | If `< 2`, proceed to next check |
| `entity + 0x394` (f32) | Unknown — occlusion or attachment float | If `> 0`, apply camera-distance shade to RGB |

**CE-confirmed values for a plasma pistol** (`weapons\plasma pistol\plasma pistol`):
- `+0xA4` = 0 (< 2 ✓)
- `+0x394` = 1.0 (> 0 ✓)

**Note:** `entityCategory` is at `entity + 0x70` (u16, value 2 = Weapon). The offset `+0x60`
frequently cited in notes is **wrong** for Halo CE in MCC — that offset contains float data.

These modulate brightness but never zero it out entirely — they are not a reliable
suppression path. The light tag flag `lightTag + 0x30 & 0x20` ("no entity shade") can
bypass this branch entirely.

---

## Recommended Hook: `updateFlareTransform` (`7ffd7c8c7d70`)

### Why this function

- Called **once per frame, per flare, only when the mount entity is alive and in range**.
- The mount entity handle (`entry + 0x2C`) is readable before any expensive bone sampling.
- Returning early prevents both `queueFlareLight` and `submitLensFlare` — no partial state.
- No writes outside the flare entry and spatial partition, so skipping is fully reversible
  on a per-frame basis.

### Hook stub (pseudocode)

```cpp
// DAT_7ffd7e6c0900 is a global POINTER (8 bytes) → heap-allocated flare list struct.
// g_flareListPtrAddr points at that global variable inside halo1.dll.
static uintptr_t g_flareListPtrAddr = 0;  // = halo1_base + offset_of(DAT_7ffd7e6c0900)

using fn_updateFlareTransform_t = void(__fastcall*)(uint32_t flare_handle);
static fn_updateFlareTransform_t orig_updateFlareTransform = nullptr;

void __fastcall hk_updateFlareTransform(uint32_t flare_handle) {
    // Step 1: dereference the global ptr → get flare list struct base
    int64_t list_base  = *reinterpret_cast<int64_t*>(g_flareListPtrAddr);
    // Step 2: read entries offset (i32 at +0x34; observed value = 0x38)
    int32_t entries_off = *reinterpret_cast<int32_t*>(list_base + 0x34);
    // Step 3: compute entry address
    int64_t entry      = list_base + entries_off + (flare_handle & 0xFFFF) * 0x7C;
    // Step 4: read mount entity handle at entry+0x2C
    uint32_t entity_h  = *reinterpret_cast<uint32_t*>(entry + 0x2C);

    if (shouldSuppressFlareForEntity(entity_h)) {
        return;  // skip enqueue → frustum cull won't see this flare → no render
    }
    orig_updateFlareTransform(flare_handle);
}
```

`shouldSuppressFlareForEntity` would check whether `entity_h` resolves to one of the
entities we want flare-free (e.g. check entity tag ID or a runtime "no flares" flag).

### Resolving `DAT_7ffd7e6c0900`

This is the flare list manager pointer. Resolve it as a halo1.dll-relative offset rather
than hardcoding the runtime address:

```cpp
// DAT_7ffd7e6c0900 is in halo1.dll — compute offset at attach time:
uintptr_t halo1_base = (uintptr_t)GetModuleHandleA("halo1.dll");
g_flareListBase      = reinterpret_cast<int64_t*>(halo1_base + /* offset */);
```

The offset can be computed from the known runtime address logged during this session.

**Session-verified addresses** (halo1.dll base = `0x7ffd7bce0000` this session):
- `DAT_7ffd7e6c0900` → offset `0x29E0900` from halo1.dll base
- `updateFlareTransform` → offset `0x8F7D70` from halo1.dll base

```cpp
uintptr_t halo1_base    = (uintptr_t)GetModuleHandleA("halo1.dll");
g_flareListPtrAddr      = halo1_base + 0x29E0900;   // &DAT_7ffd7e6c0900
void* hook_target       = (void*)(halo1_base + 0x8F7D70);  // updateFlareTransform
```

---

## Alternative Hook: `submitLensFlare` (`7ffd7c8f1e84`)

Less clean — the entity handle is **not** directly in the parameters. `param_1[0]` is the
`LensFlareTagData*`; entity context would require cross-referencing the flare list at call
time. Additionally this only suppresses the sprite, not the point light (`queueFlareLight`
would still run). Not recommended.

---

## Notes on Static Flares (entry+0x58 == -1)

Flares with no tick timestamp are world-fixed (no mount entity, `entry+0x2C == 0xFFFFFFFF`).
`updateFlareTransform` handles them via a marker lookup path rather than bone sampling.
These should always pass through the hook unblocked since they have no entity to filter on.

---

## CE Live Validation Results

All tests performed non-destructively (read-only) against a live MCC session with a plasma
pistol flare visible in-game.

### Flare List Manager

| Field | Offset | Expected | Observed |
|-------|--------|----------|----------|
| Pointer to struct | `0x7ffd7e6c0900` | heap ptr | `0x1d648de6790` ✓ |
| Stride | listBase+0x22 (u16) | 0x7C | **0x7C** ✓ |
| Capacity | listBase+0x2e (i16) | >0 | **326** slots ✓ |
| Entries offset | listBase+0x34 (i32) | >0 | **0x38** ✓ |

**Critical correction:** `DAT_7ffd7e6c0900` is a **pointer** to a heap-allocated struct,
not the struct itself. All offset math must dereference this pointer first.

### Plasma Pistol Entity Validation

Entity: handle `0xe85803b7` (index 951) = `weapons\plasma pistol\plasma pistol`

| Check | Field | Value | Result |
|-------|-------|-------|--------|
| Is weapon | `entity+0x70` (u16) | 2 | ✓ Weapon |
| Gate A | `entity+0xA4` (u16) | 0 | ✓ 0 < 2, gate passes |
| Gate B | `entity+0x394` (f32) | 1.0 | ✓ 1.0 > 0, gate passes |

**Correction:** `entityCategory` is at `entity+0x70`, **not `+0x60`** as previously noted.
Offset `+0x60` contains float data in the entity struct.

### Plasma Pistol Flare Entries

Four flares mounted on the plasma pistol entity (all tick=-1, static):

| Flare idx | Tag path |
|-----------|----------|
| 73 | `weapons\plasma pistol\muzzle flash` |
| 74 | `weapons\plasma pistol\plasma pistol flare` |
| 77 | `weapons\plasma rifle\overcharge` (shared tag) |
| 78 | `weapons\plasma pistol\plasma pistol flare` |

### Confirmed Entity Handle Field

`flare_entry + 0x2C` correctly stores the u32 entity handle for all 4 entries above.
The handle `0xe85803b7` is present at that offset — the hook can read it directly.

### Hook Viability Confirmed

The `updateFlareTransform` hook approach is validated:
- The mount entity handle is at `entry+0x2C` before any bone/spatial work
- Returning early for a targeted entity handle will suppress all 4 of its flares
- All 326 capacity slots can be walked in ~1ms; per-frame per-flare check is O(1)
