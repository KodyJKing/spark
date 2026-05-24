# Damage System

Analysis of Halo MCC (H1) damage processing. Based on static analysis of `damageEntity01` and `damageEntity02`. Items marked **[UNVERIFIED]** are hypotheses for dynamic testing.

---

## Call Chain

```
caller
  └─ damageEntity02 @ 7ffd7697ea28   ← entry point; takes DamageEvent*
       ├─ damageEntity02 (recursive)  ← for shielded vehicle passengers
       └─ damageEntity01 @ 7ffd7697fbd0  ← called directly per-entity; not via shared state
```

`damageEntity02` calls `damageEntity01` with parameters assembled on its own stack. There is no global state set up beforehand — canceling `damageEntity02` fully prevents `damageEntity01` from running for all targets. This makes `damageEntity02` the correct hook point for the modding API.

---

## DamageEvent Struct

Defined in `smc64/src/engine/types/damage_event.hpp`. Total size reported by Ghidra: **256 bytes (0x100)**.

| Offset | Type | Name | Confidence | Notes |
|--------|------|------|-----------|-------|
| 0x00 | `uint32_t` | `damageTypeTagHandle` | **Confirmed** | Tag handle for the damage effect definition |
| 0x04 | `uint32_t` | `sourceType` | **Confirmed** | Per-weapon object type index — **not** a coarse category enum. Confirmed dynamically: PlasmaPistol=`0x11`, PlasmaRifle=`0x12` (consecutive, confirming weapon-archetype granularity). Value `4` in the vehicle-seat check is likely a specific vehicle type ID, not a generic vehicle category. Name may be misleading; semantics closer to "source object type ID". `sourceTypeIndex` at `0x14` is now the primary candidate for coarse damage category. **[UNVERIFIED: melee, fall, environment/no-attacker values]** |
| 0x08 | `uint32_t` | `flags` | High | Bitfield — see below |
| 0x0c | `uint32_t` | `interactorHandle` | High | Entity handle looked up in InteractThings; cleared to `0xFFFFFFFF` if stale |
| 0x10 | `uint32_t` | `attackerHandle` | High | Source entity handle passed to `getTypedEntityPointer` |
| 0x14 | `int16_t` | `sourceTypeIndex` | Medium | Compared to `-1` (none) and `7` (unknown); used to look up per-source damage modifiers. Now a **higher-priority** candidate for coarse damage category (bullet / melee / grenade / vehicle / fall), given that `sourceType` at `0x04` turned out to be per-weapon granularity. **[UNVERIFIED]** |
| 0x16 | `uint8_t[0x16]` | `_unk0x16` | Unknown | 22 bytes completely unaccounted for |
| 0x2c | `Vec3` | `hitPosition` | **Confirmed** | World-space position of the hit point; verified dynamically (was initially swapped with hitDirection) |
| 0x38 | `Vec3` | `hitDirection` | **Confirmed** | Direction vector of the incoming damage; verified dynamically |
| 0x44 | `float` | `baseDamage` | High | Read in the randomized damage lerp calculation |
| 0x48 | `float` | `damageMultiplier` | High | Written by the vehicle-occupant loop (divided by occupant count) |
| 0x4c | `float` | `resultHealth` | Medium | Written at end of per-entity loop; likely output **[UNVERIFIED: read vs. write]** |
| 0x50 | `uint16_t` | `materialType` | Medium | Written from per-entity tag data; appears to communicate hit surface to caller **[UNVERIFIED]** |

### flags Bitfield (0x08) — [UNVERIFIED]

| Bit | Mask | Hypothesis | Rationale |
|-----|------|-----------|-----------|
| 0 | `0x001` | Single-entity target | When set, entity list is not walked — only the direct target is damaged |
| 2 | `0x004` | Instant kill | `applyDamage` flag in 01 forced; entity HP zeroed |
| 5 | `0x020` | Headshot | Set/cleared around recursive `damageEntity02` calls for vehicle passengers |
| 8 | `0x100` | Unknown | Propagated into `notifyFlags` as the shield-hit bit |

---

## damageEntity02 — Entry Point

**Address:** `7ffd7697ea28`

### Responsibilities (high confidence)
1. **Validates** `interactorHandle` — clears it if the entity no longer exists in InteractThings.
2. **Randomizes damage** — lerps between `tagData.minDamage` and `tagData.maxDamage` using an LCG (`DAT_7ffd78c874f8 = DAT_7ffd78c874f8 * 0x19660d + 0x3c6ef35f`), scaled by `event->baseDamage`.
3. **Applies attacker modifier** — if attacker entity has a `damage_modifier` tag (fields `0x1c8` or `0x1c0`), calls `_applyAttackerDamageModifier`.
4. **Scales for team damage** — SP: calls `_getLocalPlayerDamageScale`. MP: calls `getTeamDamageMultiplier(attackerTeam, targetTeam)`.
5. **Builds entity list** — if `flags & 5 == 0`, walks the entity's parent-child chain. Appends mounted turret (if any). Otherwise targets only the direct entity.
6. **Recurses for vehicle passengers** — if target is a vehicle with shield-type passengers, calls itself with `flags |= 0x20`.
7. **Dispatches effects** — calls `_spawnDamageEffect` per entity using `impactPosition` and camera index (behavior varies by `DAT_7ffd789036e0` split-screen mode).
8. **Calls damageEntity01** per entity.
9. **Assembles notify flags** and calls `dispatchDamageNotifications` — kills, scoring, medals.
10. **Floodcarrier special case** — uses `strstr(getTagName(tagId), "floodcarrier")` to queue a deferred explosion event in a circular buffer.

### Uncertainties
- What callers exist? Xrefs not yet checked. **[TEST: set breakpoint, log call stack across different damage sources]**
- Is `boneIndex` (param 5, `int16_t`) a bone index or a region index? It is passed through to `damageEntity01` as `regionIndex`. **[TEST: trigger damage to specific body parts and log the value]**

---

## damageEntity01 — Per-Entity Application

**Address:** `7ffd7697fbd0`

### Responsibilities (high confidence)
1. **Resolves entity pointer** from handle via entity array.
2. **Zeroes damage** if target is an empty vehicle (category 1, no driver at `field_0x304`) and the damage effect has the vehicle-passthrough flag (`damageEffectTagData & 0x40`).
3. **Computes shield drain rate** — `1.0 / getEntityShields(handle)` clamped to `[0, ∞)`.
4. **Applies body-region damage** — accumulates hit points per region index; if threshold exceeded, calls `destroyEntityRegion`.
5. **Applies HP reduction** — `entity->field_0x9c -= scaledDamage` (when `applyDamage == '\x01'`).
6. **Accumulates damage totals** — `field_0xa8` (this-tick) and `field_0xb4` (cumulative), clamped to 1.0.
7. **Clamps HP at 0** if cheats-enabled global is set and certain vehicle/turret conditions are met.
8. **Death check** — if HP < 0: calls `killEntity`, then `_processKillEffects`.
9. **Shield depletion check** — if health crosses tag-defined shield threshold: calls shield depletion script (`field_0xa4` tag ref).
10. **Melee response** — if damage effect has the melee-response flag and entity has a melee-response tag, calls `_spawnDamageEffect` variant.
11. **Notifies** via `_broadcastEntityDamaged(entityHandle, 0x40)`.

### Key parameter: `applyDamage` (param 13)
- `'\x01'` — full application: modifies HP, region damage, triggers callbacks.
- `'\x00'` — dry run: computes damage values and writes to output pointers but **does not modify entity state**. **[UNVERIFIED: confirm no state is written in dry-run path]**

---

## Helper Functions

| Address | Name | Confidence | Notes |
|---------|------|-----------|-------|
| `7ffd7697d000` | `getEntityHealth` | High | Returns `float` |
| `7ffd7697d0a0` | `getEntityShields` | High | Returns `float`; unclear if normalized or raw HP **[UNVERIFIED]** |
| `7ffd76981344` | `destroyEntityRegion` | High | Args: `(entityHandle, regionIndex)` |
| `7ffd7697d9e0` | `killEntity` | High | Args: `(entityHandle)` |
| `7ffd7697ddd4` | `_onEntityKilledByExplosion` | Medium | Called in kill path only when explosion-path flag set; may be more general |
| `7ffd769807f4` | `dispatchDamageNotifications` | High | Args: `(entityHandle, event*, notifyFlags, damageAmount)` |
| `7ffd769167c4` | `_processKillEffects` | Medium | Called when `notifyFlags & 4`; exact scope unclear |
| `7ffd7697e6dc` | `getEntityTeam` | High | Returns team index |
| `7ffd768863fc` | `getTeamDamageMultiplier` | High | Args: `(teamA, teamB)` → `float` |
| `7ffd769179a4` | `isPlayerControlled` | High | Returns `char` (`'\x01'` = player) |
| `7ffd7687b25c` | `getTagName` | High | Returns `char*`; used with `strstr` |
| `7ffd76a65868` | `_getLocalPlayerDamageScale` | Medium | Called in both SP and MP paths with different args; may be more general than named |
| `7ffd769aca88` | `_applyAttackerDamageModifier` | Medium | Modifies `computedDamage` in-place via pointer |
| `7ffd7698b708` | `_spawnDamageEffect` | Medium | Args: `(cameraIndex, event*, impactPosition*, baseDamage)` |
| `7ffd769e9fac` | `_broadcastEntityDamaged` | Medium | Args: `(entityHandle, 0x40)`; second arg always literal |

---

## Hypotheses for Dynamic Testing

Priority order:

1. ~~**Confirm `hitDirection` vs `impactPosition` at 0x2c / 0x38**~~ — **RESOLVED.** Fields were swapped from initial analysis. `hitPosition` at 0x2c, `hitDirection` at 0x38. Verified dynamically.

2. ~~**Enumerate `sourceType` enum**~~ — **PARTIALLY RESOLVED.** Field is a per-weapon object type index, not a coarse category enum. Confirmed dynamically: PlasmaPistol=`0x11`, PlasmaRifle=`0x12`. Consecutive values for related weapons confirm it tracks weapon archetype rather than delivery mechanism. Value `4` in the vehicle-seat check likely refers to a specific vehicle type ID. Outstanding: melee, fall damage, and environment (no attacker) values still unknown. `sourceTypeIndex` at `0x14` is now the higher-priority candidate for coarse category and should be added to the next logging script.

3. **Enumerate `flags` bits** — headshot vs. body, explosive vs. direct, friendly fire. Log `event->flags`.

4. **Confirm `applyDamage` dry-run** — set a breakpoint at the `entity->field_0x9c -=` line; it should only be hit when `applyDamage == '\x01'`.

5. **Confirm `resultHealth` is output** — read `event->resultHealth` after `damageEntity02` returns and compare to `getEntityHealth(entityHandle)`.

6. **Identify `_unk0x16`** — dump the 22 bytes across multiple damage events; look for patterns (could be caller info, hit bone index, physics impulse data, or script context).

7. **Confirm `damageEntity02` is the outermost entry** — log call stack at entry to `damageEntity02`; check whether any caller is itself a damage-related function. (Recursive self-calls are already accounted for.)

8. **Verify `getEntityShields` is normalized** — read its return value alongside a known shield percentage from the HUD.
