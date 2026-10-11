# Halo CE Audio (FMOD4 / WASAPI)

> **Confidence note:** This document is based on a session that was compacted at least twice. Confidence levels are conservative as a result.
> 
> **VA note:** halo1.dll is frequently rebased in Ghidra to match the live process. Any raw VAs in this document are session snapshots only and will be stale after the next rebase. Refer to Ghidra labels (`_fmod_wasapi_init`, etc.) as stable identifiers; use the offset script to resolve them to current VAs when needed.

---

## Overview

Halo CE (`halo1.dll`) uses **FMOD4** as its audio middleware. FMOD4 is compiled statically into the DLL. At runtime, FMOD4 initializes a **WASAPI** output backend (not XAudio2, not DirectSound). Evidence:

- Source path embedded in binary: `d:\projects\mcc\main\h1\code\common\lib_3dpart\src\fmod\fmod4\win\src\fmod_output_wasapi.cpp`
- All other FMOD output backends are also present (`fmod_output_dsound.cpp`, `fmod_output_winmm.cpp`, `fmod_output_asio.cpp`, etc.), but WASAPI is the active one in practice.
- `AUDIOSES.DLL` (Windows WASAPI implementation) is loaded at runtime; COM vtables for `IAudioClient` etc. originate there.

**Confidence: HIGH** (source paths are unambiguous; confirmed via AUDIOSES.DLL module enumeration at runtime).

---

## Why XAudio2 Hook Fails

An early approach attempted to intercept Halo audio by patching the `IXAudio2SourceVoice::SubmitSourceBuffer` vtable entry.

**Result:** The hook fired 79,385 times for a single voice at address `0x241584F5F00` — a looping audio source cycling through buffer indices 0→98, repeating ~802 times. This was identified as **SMC64's own SM64 audio voice**, not Halo's audio.

`s_voices` (our tracking set) remained empty for the entire run because every call matched `s_ownVoice`. **FMOD4 never touches XAudio2.** It calls WASAPI directly via COM interfaces.

**Conclusion:** XAudio2 vtable hook is a dead end for Halo audio. **Confidence: HIGH.**

---

## FMOD4 WASAPI Init Function

| Property | Value |
|---|---|
| Ghidra name | `_fmod_wasapi_init` |
| Confidence | MEDIUM (2 compactions; identified by WASAPI COM call pattern) |

### What it does

1. Calls `CoCreateInstance(CLSID_MMDeviceEnumerator, ...)` to obtain `IMMDeviceEnumerator`
2. Calls `IMMDeviceEnumerator::GetDevice(deviceId, &pMMDevice)` using a device ID string stored in the context at `pWasapiCtx + 600 + deviceIndex * 0x10`
3. Calls `__fmod_wasapi_getDeviceInfo` to populate device endpoint info at `pWasapiCtx + 0x458`
4. Calls `IMMDevice::Activate(IID_IAudioClient, CLSCTX_ALL, NULL, &pAudioClient)` — stores result at `pWasapiCtx + 0x468`
5. Calls `IAudioClient::GetMixFormat` to determine the mix format; stores format info (sample rate, channel count) at various offsets
6. Calls `IAudioClient::IsFormatSupported` / selects closest format
7. Calls `IAudioClient::Initialize` with the WAVEFORMATEXTENSIBLE; creates a waitable timer at `pWasapiCtx + 0x488`
8. Calls `IAudioClient::GetBufferSize` → stores frame count at `pWasapiCtx + 0x478`
9. Calls `IAudioClient::GetService(IID_IAudioRenderClient, &pRenderClient)` — stores result at `pWasapiCtx + 0x470`
10. Allocates mix buffers via `_fmod4_alloc`, stores at `+0x750`, `+0x778`, `+0x788`
11. Calls `_fmod4_createThread` to spawn the audio render thread (`_fmod_wasapi_audioThread` label)

### WASAPI Context Struct (`pWasapiCtx`) — Partial Layout

All offsets confirmed from decompiler output unless noted otherwise.

| Offset | Size | Type | Description | Confidence |
|---|---|---|---|---|
| `+0x24b` | 1 | bool | Exclusive mode flag (0=shared, 1=exclusive) | MEDIUM |
| `+0x458` | ? | struct | Device endpoint info (filled by `__fmod_wasapi_getDeviceInfo`) | MEDIUM |
| `+0x468` | 8 | `IAudioClient*` | COM audio client interface | HIGH |
| `+0x470` | 8 | `IAudioRenderClient*` | COM render client interface | HIGH |
| `+0x478` | 4 | `UINT32` | Buffer frame count (from `GetBufferSize`) | HIGH |
| `+0x480` | 4 | `UINT32` | Channel count | HIGH |
| `+0x484` | 4 | `UINT32` | Actual sample rate (from mix format) | HIGH |
| `+0x488` | 8 | `HANDLE` | Waitable timer (created with `CreateWaitableTimer`) | HIGH |
| `+0x600` | 16 each | device list | Array of device entries (ID strings), indexed by `deviceIndex` | MEDIUM |
| `+0x750` | 8 | `void*` | Primary mix buffer | MEDIUM |
| `+0x770` | 8 | `void*` | Pointer to FMOD multichannel thread context | MEDIUM |
| `+0x778` | 8 | `void*` | Secondary mix buffer | MEDIUM |
| `+0x788` | 8 | `void*` | Tertiary mix buffer | MEDIUM |

### GUID Data Labels

| Ghidra name | Description | Confidence |
|---|---|---|
| `DAT_CLSID_MMDeviceEnumerator` | CLSID passed to `CoCreateInstance` | HIGH |
| `DAT_IID_IMMDeviceEnumerator` | IID for `CoCreateInstance` output | HIGH |
| `DAT_IID_IAudioClient` | IID passed to `IMMDevice::Activate` | HIGH |
| `DAT_IID_IAudioRenderClient` | IID passed to `IAudioClient::GetService` | HIGH |
| `DAT__fmod_wasapi_subformat_pcm` | WAVEFORMATEXTENSIBLE SubFormat (PCM branch) | MEDIUM |
| `DAT__fmod_wasapi_subformat_float` | WAVEFORMATEXTENSIBLE SubFormat (float branch, when format==5) | MEDIUM |

---

## Audio Render Thread

| Property | Value |
|---|---|
| Ghidra label | `_fmod_wasapi_audioThread` (disassembly comment; not yet defined as a function) |
| Confidence | MEDIUM |

This address is passed as the thread proc to `_fmod4_createThread` at the end of `_fmod_wasapi_init`. The thread runs the WASAPI audio pump loop, calling `IAudioRenderClient::GetBuffer` and `ReleaseBuffer` each cycle.

---

## Pitch-Shifting Approach (Tabled)

**Goal:** Shift the pitch of all Halo CE audio during bullet-time (slow-motion mod) to match the slowed tempo.

### Proposed Method

Use [`IAudioClockAdjustment::SetSampleRate`](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclockadjustment-setsamplerate) on the live `IAudioClient*` at `pWasapiCtx + 0x468`.

```
// Pseudocode:
IAudioClient* pAudioClient = *(IAudioClient**)(pWasapiCtx + 0x468);
IAudioClockAdjustment* pAdj = nullptr;
pAudioClient->GetService(IID_IAudioClockAdjustment, (void**)&pAdj);
pAdj->SetSampleRate(normalSampleRate * bulletTimeScale);
```

This adjusts the WASAPI clock so audio is consumed at a different rate, effectively pitch-shifting (and time-stretching) all audio that FMOD4 writes through this client.

**Caveats / Open Questions:**
- Whether Windows shared-mode WASAPI actually pitch-shifts via this, or whether it only changes buffer timing, is **unverified**. Dynamic testing would be needed.
- `IAudioClockAdjustment` requires `AUDCLNT_STREAMFLAGS_RATEADJUST` to be set when `IAudioClient::Initialize` is called. FMOD4's init call would need to be patched to include this flag. This is non-trivial without modifying the init path.
- Alternatively, could use a custom FMOD4 output plugin or hook the FMOD4 DSP/mix pipeline, but those paths would require much deeper FMOD4 internals reversing.

### Finding the Live Context Pointer

To locate `pWasapiCtx` at runtime, any of these approaches work:

1. **One-shot breakpoint on `_fmod_wasapi_init` entry** — fires once at startup; `RCX` = `pWasapiCtx`. Note the address and use a pointer scan from there.
2. **Hook the audio render thread** (`_fmod_wasapi_audioThread` label) — RCX likely carries the context on entry.
3. **Hardware BP on `IAudioRenderClient::GetBuffer`** — fires every audio cycle; walk the call stack back to recover `pWasapiCtx`.

**Status: TABLED.** The `AUDCLNT_STREAMFLAGS_RATEADJUST` requirement means patching the init call is necessary before any runtime `SetSampleRate` attempt will succeed. This adds scope. Revisit if audio pitch-shifting becomes a priority again.

---

## AUDIOSES.DLL (Reference)

`AUDIOSES.DLL` is the Windows WASAPI session API implementation. COM vtables for `IAudioClient`, `IAudioRenderClient`, etc. live in its `.rdata` section. The `IAudioClient` vtable has ~17 methods and is likely `IAudioClient2`.

VAs for AUDIOSES are session-specific (the DLL can also rebase) and are not recorded here. Use CE's module enumerator or a debugger attach to locate the current base, then scan `.rdata` for the vtable.

---

## Related Files

- `smc64/src/spark/hook/HookTable.hpp` — current hook definitions; XAudio2 `SubmitSourceBuffer` hook is present but only catches SM64 voice
- `reversing/notes/DamageSystem.md` — bullet-time is triggered from the damage system context
