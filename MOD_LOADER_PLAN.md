# Mod Loader / SDK Plan

Goal: pull the Mario mod out of `spark.dll` into its own `MarioMod.dll`, and have Spark
load mods from a `/mods` directory next to the MCC binary
(`<MCC>/mcc/binaries/win64/mods/`), while preserving the existing save/detach/build/reinject
dev workflow regardless of whether the touched file lives in Spark or in a mod DLL.

Spark is expected to move into its own repository at some point; this plan tries not to
make assumptions that would make that harder.

## Why this is tricky: ABI

Once mod code lives in a separate DLL, anything crossing the Spark <-> mod boundary needs
a stable, compiler/CRT-version-independent ABI. `std::function`, `std::string`,
`std::vector`, `std::optional` etc. are all implementation-defined layouts and are unsafe
to pass by value across a DLL boundary unless both sides are guaranteed to be built with
the exact same compiler/STL version. Dear ImGui's globals (`GImGui`,
`GImAllocatorFunctions`) are private per-DLL statics by default, so a second DLL that
compiles its own `imgui.cpp` gets a disconnected context unless explicitly synced.

The general fix pattern used throughout this migration:
- Prefer plain C types / raw pointers / function pointers + `void*` context at any
  cross-DLL boundary.
- Where C++ convenience is still wanted on the calling side, wrap the raw C-ABI-safe
  function in a header-only template/inline wrapper so mod code keeps writing normal
  lambdas etc., but the exported symbol itself stays POD-only.
- For shared global/singleton state (event buses, hook storage), use the existing
  `SPARK_API` (`SparkAPI.h`) `dllexport`/`dllimport` convention: exactly one translation
  unit compiled with `SPARK_EXPORTS` defined provides the real instance; everyone else
  imports it. For templated storage (`Hook<Offset,Ret,Args...>`), this requires explicit
  template instantiation in one `.cpp` plus `extern template` elsewhere to suppress
  implicit (per-TU-duplicated) instantiation.

## Phase 0 — ABI hardening (spark.dll only, no mod split yet) — ✅ DONE

All of the following were verified via full solution build (0 errors) before moving on:

- [x] `Engine::foreachEntityRecord` / `foreachEntityRecordIndexed` / `foreachEntityInRadius`
      (`entity_list.hpp/.cpp`) — `std::function` → raw fn-ptr + `void*` ctx, with a
      header-only template wrapper preserving lambda call-site syntax.
- [x] `Engine::foreachFlareEntry` (`flare_list.hpp/.cpp`) — same treatment.
- [x] `Spark::Overlay::ESP::drawText` / `Spark::Overlay::Gizmos::drawText` —
      `std::string` → `const char*`.
- [x] `Spark::teleportPlayer` (`events/TeleportPlayer.hpp/.cpp`) — added `SPARK_API`
      export/import annotation to the single shared instance.
- [x] `Spark::onRenderPauseMenuTabs` / `onRenderDebugUI` / `onRenderDebugWorld`
      (`RenderBuses.hpp` + new `RenderBuses.cpp`) — converted from `inline` (implicit
      per-DLL duplication) to `SPARK_API extern` + single definition.
- [x] `Spark::Hook<Offset, Ret, Args...>` (`hook/Hook.hpp`, `hook/Hooks.hpp`, new
      `hook/HookInstantiations.cpp`) — converted per-instantiation `inline static`
      storage into a single exported/imported instance per hook via explicit template
      instantiation + `extern template` suppression. Known-noisy side effect: `LNK4217`
      ("locally defined symbol imported") warnings appear for every `SPARK_API` symbol
      while everything is still linked into one binary — expected, harmless, and
      resolves naturally once `MarioMod.dll` is a real separate binary. `C4251` on the
      `Hook<>` struct (member `EventBus<>` lacking its own dll-interface) is suppressed
      locally via `#pragma warning(disable: 4251)` with a comment explaining why it's
      safe (the shared storage guarantee comes from the explicit-instantiation scheme,
      not from `EventBus<>` itself being export-annotated).
- [x] ImGui context/allocator bridge — `spark_getImGuiContext()` /
      `spark_getImGuiAllocatorFunctions()` added to `SparkAPI.h/.cpp`; new header-only
      `spark/mod/ImGuiBridge.hpp` with `Spark::Mod::syncImGuiContext()` for mod code to
      call before any `ImGui::` call (e.g. at the top of every render-bus handler).

### Deferred (lower priority, not yet touched)

These return non-POD types across what will become the mod boundary, but nothing
outside Spark currently needs to call them, so they're left as-is until something
actually does:
- `Entity::copyBoneTransforms()` — returns `std::vector<QuatTransform>`.
- `Tag::groupIDStr()` — returns `std::string`.
- `Engine::Scripting::TerminalOutput::poll()` — returns `std::vector<Entry>`.
- `player.hpp`'s `getPlayerPosition()` / `getPlayerVelocity()` — return
  `std::optional<Vec3>`. Explicitly agreed acceptable to leave for now.

### Still to do before/alongside Phase 1

- [ ] Export `Spark::halo1` (currently a private, non-exported global in `Spark.cpp`) via
      a `spark_getHalo1Base()` accessor in `SparkAPI.h/.cpp`.
- [ ] Export whatever `Engine::` query functions Mario mod needs (`isGameLoaded`,
      entity/object query helpers, etc.) — grow this list incrementally as link errors
      surface once `MarioMod.dll` actually exists and is building against real code.
- [ ] Add a lightweight ABI version guard mods must export (e.g. `spark_getAbiVersion()`),
      to be checked by the mod loader before calling into a mod DLL.

## Phase 1 — Extract `MarioMod.dll`

- New premake project (`smc64-mariomod/` or similar), mirroring `smc64`'s project
  settings: C++20, `staticruntime "off"` (critical — keeps CRT/heap shared with Spark
  and any other mod DLLs), same preprocessor defines.
- Move `smc64/src/mods/mario/**` into the new project.
- Link `libsm64` directly (mods can each link their own copy of libsm64/other static
  libs; only things that hold shared mutable state need to go through Spark's exports).
- Link against `spark.lib` (the import lib generated by `spark.dll`) for `SparkAPI.h`
  calls, hook registration (`Spark::HookName::addHandler(...)`), event buses, and
  `Engine::` query functions.
- Compile its own private copy of core ImGui (no backends — Spark owns the actual
  Win32/DX11 backend and render loop) using the same vendored commit/`imconfig.h` as
  Spark, and call `Spark::Mod::syncImGuiContext()` at the top of every render-bus
  handler.
- Replace the current `registry.add(new MarioMod())` call in `Spark.cpp` with a
  `spark_modLoad()`-style exported entry point convention that `MarioMod.dll` implements
  and the loader (Phase 2) calls.

## Phase 2 — Mod loader

New `spark/mod/ModLoader.hpp/.cpp`:
- Resolve `<exe-dir>/mods/` via `GetModuleFileNameA(NULL, ...)` on the host process
  (works whether Spark is injected into the real MCC exe or run via the dev
  launcher/test harness, since it's always relative to the actual running exe, not
  Spark's own DLL path).
- Enumerate `*.dll` in that directory.
- For each: `LoadLibrary`, validate the ABI version export, then call its
  `spark_modLoad()` entry point. Track the returned `HMODULE` for teardown.
- Wire into `Spark::init()` / `Spark::free()`. Critical teardown ordering:
  `registry.freeAll()` → `FreeLibrary` every loaded mod module → *then* Spark's own
  MinHook/Overlay teardown. Doing this out of order risks a locked mod DLL blocking a
  rebuild, or Spark tearing down hooks/state a mod's `free()` still depended on.

## Phase 3 — Dev workflow / packaging

- Add a copy-step script (mirrors the existing `build_libsm64.ps1` staging pattern) to
  copy built mod DLLs (`MarioMod.dll` + `.pdb`) into
  `<MCCPath>/mcc/binaries/win64/mods/` after each build.
- Update `package.ps1` / `install_package.ps1` similarly for end-user installs.
- No changes needed to the core detach → build → reinject loop itself
  (`watch.ps1` / `build.ps1` / `run_launcher.ps1` / `kill_injected_instances.ps1`) — the
  loop already rebuilds+relaunches the whole solution and doesn't care how many DLLs
  are involved, as long as the mods directory is populated before injection.
