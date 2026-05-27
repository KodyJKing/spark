# Camera System

Investigation triggered by: MarioCamera jitter / interpolation quality.

## Tick vs Render Frame Architecture

`_gameUpdateFrame` (`0xB0DD10`) is the main per-render-frame entry point. Its structure is:

```
_gameUpdateFrame(renderDelta):
    tickBudget = _computeTickBudget(renderDelta)   // how many ticks this frame
    for i in range(tickBudget):
        <entity / physics / logic ticks>
        tick_counter++
    wallDelta = currentTime - lastFrameTime        // true wall-clock render delta
    updateCameras(wallDelta)                       // ONCE per render frame, post-tick
    _finalizeCameraForRender(wallDelta)
    updateParticles(...)
```

**`updateCameras` (`0xB14380`) is called once per render frame, not once per tick.**
The `float dt` parameter it receives (and that flows into the `UpdateCamera` hook) is the
real wall-clock render delta — not a fixed tick step.

## Camera Update Pipeline

```
updateCameras(renderDelta)
  -> stores renderDelta into DAT_cam_deltaTimeSeconds
  -> for each active camera:
       buildCameraInputParams(cameraIndex, inputParams)
       updateCameraTransitionState(...)
       (*cameraUpdateFn)(cameraState, inputParams, &outCamera)   // per-type dispatch
       blend timer -= renderDelta                                 // delta-time aware
       finalizeCameraOutput(cameraIndex, &outCamera)             // store pointer

_finalizeCameraForRender(renderDelta)
  -> for each active camera:
       _copyCameraToRenderSlot    // copy output, cap FOV scalars (5 components)
       _updateCameraRenderState   // update delta buffer; skip if renderDelta == 0
       _buildRenderCameraOutput   // final direction/position for renderer
```

## What IS Interpolated

| System | Method | Delta-time aware? |
|---|---|---|
| Camera type blend/transition timers | `blendTimer -= renderDelta` | Yes |
| FOV / scalar fields | Per-component clamping in `_copyCameraToRenderSlot` | Yes (render dt stored) |
| Aim-assist / zoom tracking (`_updatePlayerZoomAimAssist`) | Lerp with per-field blend rate from tag data | Per-tick only |

## What is NOT Interpolated

**Camera position is NOT sub-tick interpolated.** The per-camera update functions
(`cameraUpdateFn` dispatch) read entity positions directly from the entity array.
Entity positions only change during game ticks. Between ticks (i.e., extra render
frames at high FPS), `updateCameras` runs but sees the same entity world positions —
resulting in a stepped/snapped camera position at tick boundaries.

`_buildRenderCameraOutput` does compute a delta buffer (prev vs current camera state)
and uses it for direction normalization, but does **not** use it to lerp the position
across the render frame.

## Sub-tick Accumulator (non-actionable externally)

`_computeTickBudget` maintains a fractional-time accumulator at
`*(float *)(DAT_7fff4becfd68 + 0x1c)`. It correctly computes the sub-tick remainder
but **zeroes it before returning**. The engine intentionally discards the sub-tick
remainder each frame — there is no exposed "how far through the current tick" value.

## Key Globals

| Global | Offset | Description | Confidence |
|---|---|---|---|
| `DAT_cam_deltaTimeSeconds` | `0x7A0EE8` (image-relative) | Wall-clock render delta stored at start of `updateCameras` | High |
| `DAT__tickRateBase` | `0x7fff4a843170` | Base tick rate constant (likely 30.0); multiplied by speed scale to get effective ticks/sec | Medium |
| `DAT__lastFrameWallClockTime` | `0x7fff4bdc1338` | Wall-clock timestamp of previous `_gameUpdateFrame` call; delta gives true render dt | Medium |
| `Camera_ARRAY_7fff4bdcb9c0` | — | Per-camera output array, stride 0xF8 | (already named) |
| `DAT_7fff4bdcbd60` | — | Per-render-camera slot: holds pointer to camera output + render state, stride 0x2A8 | — |
| `DAT_7fff4bdcbd50` | — | Render delta stored by `_finalizeCameraForRender` for downstream use | — |

## Key Functions

| Function | Offset | Description | Confidence |
|---|---|---|---|
| `_gameUpdateFrame` | `0xB0DD10` | Main per-render-frame entry: tick loop → updateCameras → finalize | Medium |
| `updateCameras` | `0xB14380` | Per-frame camera dispatch; stores render delta globally | High (hook site) |
| `_computeTickBudget` | `0xB0D7D8` | Computes tick count for this frame; discards sub-tick remainder | Medium |
| `_finalizeCameraForRender` | `0xB15F30` | Post-updateCameras render prep for all active cameras | Medium |
| `_buildRenderCameraOutput` | `0xB17288` | Final direction/position assembly for renderer | Medium |
| `_copyCameraToRenderSlot` | `0xB1625C` | Copies camera output to render slot; clamps FOV scalars | Medium |
| `_updateCameraRenderState` | `0xB16334` | Updates delta buffer; decrements blend timers | Medium |
| `_computeCameraStateDelta` | `0xB17164` | Computes 8-float delta between previous and current camera state | Medium |
| `_updatePlayerZoomAimAssist` | `0xC74560` | Per-tick zoom/aim-assist manager; uses lerp with tag-defined blend rates | Medium |
| `_getPlayerCameraWorldPos` | `0xB25010` | Resolves player camera world position (delegates to seat/vehicle logic) | Medium |
| `_resolveSeatCameraPos` | `0xB24D00` | Resolves camera position for entity seat/vehicle occupation | Medium |
| `finalizeCameraOutput` | `0xB15ED0` | Stores camera pointer into render slot; handles first-frame init | High |

## Implications for MarioCamera

The current `MarioCamera::getCameraPosition()` uses frame-counting
(`framesSinceLastUpdate % effectiveInterval`) to extrapolate position.  
This is fragile: it breaks silently on FPS/tickrate changes.

**Better approach:** The `float dt` parameter already arriving at the `UpdateCamera` hook
*is* the wall-clock render delta. Accumulate it since the last `onUpdate` call to get
elapsed render time, then divide by the observed tick period (1 / tickRate) to derive
`t ∈ [0, 1)`. This is accurate regardless of FPS or tick rate changes.

```cpp
// Sketch — accumulate render dt between Mario ticks
static float renderTimeAccumulator = 0.0f;
static float lastTickDuration      = 1.0f / 30.0f; // seeded from observed interval

// in onUpdate(dt):          renderTimeAccumulator = 0; lastTickDuration = dt;
// in UpdateCamera handler:  renderTimeAccumulator += hookDt;
// t = renderTimeAccumulator / lastTickDuration;  (clamp to [0,1])
```

The `float dt` arriving at `UpdateCamera` is currently labelled `unknown` in the hook
handler in `MarioCamera.cpp` — it is confirmed to be the render-frame wall-clock delta.
