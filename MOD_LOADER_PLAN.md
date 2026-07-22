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

- [x] Export whatever `Engine::` query functions Mario mod needs. **Actual scope turned
      out to be far larger than anticipated here** — effectively the entire `Engine::`
      namespace (entity/tag/map/bsp/player/raycast/scripting/flares/collision query
      functions), `Memory::isAllocated`, every math class with out-of-line methods
      (`Vec3`, `Vec3i`, `Vec4`, `Matrix3`, `Matrix4`, `Quaternion`, the free-function
      `Camera`/`Ray` pair, `Math::` free functions), and `Spark::Overlay::ESP`/`Gizmos`
      (including the mutable global `ESP::camera`). Discovered by scaffolding the real
      `smc64` project, getting it to compile, and iterating on the linker's
      unresolved-external list in batches — far faster and more accurate than manually
      auditing headers up front. For classes with out-of-line methods, exporting the
      whole class (`class SPARK_API Tag { ... }`) was used instead of per-method
      annotation. See `/memories/repo/smc64-dll-split.md` for the full list of headers
      touched.
- [ ] Export `Spark::halo1` (currently a private, non-exported global in `Spark.cpp`) via
      a `spark_getHalo1Base()` accessor in `SparkAPI.h/.cpp` — not yet needed since Mario
      mod hasn't required direct access to the raw `halo1` base pointer; revisit if a
      future link error surfaces.
- [ ] Add a lightweight ABI version guard mods must export (e.g. `spark_getAbiVersion()`),
      to be checked by the mod loader before calling into a mod DLL. Not yet implemented —
      `ModLoader::loadAll()` currently just calls `spark_modLoad()` unconditionally.

## Phase 1 — Extract `smc64.dll` (Mario mod) — ✅ DONE

Naming decision: the extracted Mario mod project is named exactly `smc64` (no
`spark-`/`MarioMod` prefix) — reviving the original project name now that the engine
core lives in the separate `spark` project.

- [x] New premake project `smc64/premake5.lua`, mirroring `spark`'s project settings:
      C++20, `staticruntime "off"` (critical — keeps CRT/heap shared with Spark), same
      preprocessor defines (`VERSION_US`, `NO_SEGMENTED_MEMORY`).
- [x] Moved `spark/src/mods/mario/**` into `smc64/src/mods/mario/**` via `git mv`
      (history preserved).
- [x] Links `libsm64` directly (`../vendor/libsm64/dist/sm64`), same as `spark` does.
- [x] Links against `spark.lib` (the import lib generated by `spark.dll`) for
      `SparkAPI.h` calls, hook registration, event buses, and `Engine::` query functions.
- [x] Compiles its own private copy of core ImGui (`vendor/imgui/*.cpp`, explicitly
      excluding `vendor/imgui/backends/*` — Spark owns the real Win32/DX11 backend), and
      calls `Spark::Mod::syncImGuiContext()` at the top of each render-bus handler in
      `MarioMod.cpp` (`RenderEntity`, `onRenderDebugWorld`, `onRenderPauseMenuTabs`).
- [x] `smc64/src/ModEntry.cpp` implements the `spark_modLoad()` exported entry point
      that `Spark.cpp` no longer calls `registry.add(new MarioMod())` directly for —
      instead `ModLoader::loadAll()` (Phase 2) finds and calls it.
- [x] `smc64/pch.h` added (force-included) to provide the same libsm64/decomp GCC-syntax
      compat shims (`VEC3I_DEFINED`, `__attribute__` no-op under MSVC) that `spark`'s own
      `pch.h` already had, plus common STL/Windows includes.
- Verified: `smc64` project compiles and links with 0 errors; `spark` project still
      builds standalone with 0 errors after the `SPARK_API` export pass.

## Phase 2 — Mod loader — ✅ DONE (minimal version)

New `spark/mod/ModLoader.hpp/.cpp`:
- [x] Resolves `<exe-dir>/mods/` via `Utils::getModsDirectory()` (`utils/Utils.hpp/.cpp`),
  which calls `GetModuleFileNameA(NULL, ...)` — the running *process'* image path, not
  `__ImageBase`/`getCurrentImageBase()` (Spark's own DLL path). This is deliberate:
  Spark is frequently injected from the repo's own build output during dev (see
  `Utils::isInjected()`), so resolving relative to Spark's own module would find the
  wrong directory. `GetModuleFileNameA(NULL, ...)` always resolves to the actual game
  executable regardless of where Spark itself was loaded from, so the mods directory is
  stable across dev (injected) and production alike.
- [x] Enumerates `*.dll` in that directory, `LoadLibraryW`s each, calls its
  `spark_modLoad()` entry point via `GetProcAddress`. Track the returned `HMODULE` for
  teardown. **Not yet implemented**: ABI version validation before calling into a mod
  DLL (see "still to do" above) — currently calls `spark_modLoad` unconditionally.
- [x] Wired into `Spark::init()` / `Spark::free()` in `Spark.cpp`, replacing the old
  direct `registry.add(new MarioMod())` call. Critical teardown ordering:
  `registry.freeAll()` → `modLoader.unloadAll()` (every mod's `free()` logic runs before
  its owning DLL is `FreeLibrary`'d) — `Spark.cpp`'s `free()` does this in the correct
  order.

## Phase 3 — Dev workflow / packaging

- [x] `scripts/copy_mods.ps1` added and wired into `scripts/build.ps1` (runs after a
      successful MSBuild): copies `bin/<Config>-Win64/smc64/smc64.dll` (+ `.pdb`) into
      `<MCCPath>/MCC/Binaries/Win64/mods/`, resolving `<MCCPath>` via `-MCCPath` param →
      `$env:MCC_HOME` → hardcoded default, matching `build_libsm64.ps1`'s convention.
- [x] Updated `package.ps1` to stage `smc64.dll` into the package's `mods/` folder
      (alongside `spark.dll` + the xaudio2 detour), and `install_package.ps1`'s
      `-Uninstall` path to remove `mods/smc64.dll`/`.pdb` (and the `mods/` directory
      itself if left empty) without touching any other mods a user may have installed.
- No changes needed to the core detach → build → reinject loop itself
  (`watch.ps1` / `run_launcher.ps1` / `kill_injected_instances.ps1`) — the loop already
  rebuilds+relaunches the whole solution and doesn't care how many DLLs are involved, as
  long as the mods directory is populated before injection.

## Status as of this writing

- `spark.dll` and `smc64.dll` both compile and link with 0 errors.
- `scripts/copy_mods.ps1` verified working for the dev workflow; `package.ps1` /
  `install_package.ps1` now also stage/remove `smc64.dll` for end-user installs.
- Still not yet done: full in-game injection/smoke test confirming `ModLoader`
  loads `smc64.dll` and Mario works end-to-end at runtime (dev workflow via
  `copy_mods.ps1` appears to be working per manual testing); the ABI version guard
  and `Spark::halo1` accessor remain deferred (see Phase 0 "still to do").
- Committed on branch `dll-split` (`93d48f3` + follow-up cleanup commits).

## Phase 4 — Repo split (`smc64.dll` moved to its own repository) — in progress

This repo (`spark`) and `smc64` (the Mario mod) are being split into two separate
repositories, with `smc64` depending on this repo as a git submodule. See `smc64`'s
`REPO_SPLIT_PLAN.md` for the full plan. As part of this:

- [x] `smc64/`, `data/` (Mario model/armature conversion), and `tags/` (`mario`,
      `jackal_shield`, `mario_cyborg`) removed from this repo — all mod-only content now
      lives exclusively in the `smc64` repo.
- [x] `vendor/libsm64` submodule removed — mod-only dependency, not needed by spark itself.
- [x] Top-level `premake5.lua` no longer includes `smc64/premake5.lua`.
- [x] `scripts/copy_mods.ps1` removed (and its call from `build.ps1`) — staging a specific
      mod's DLL during dev is now that mod repo's own concern, not spark's.
- [x] `package.ps1`/`install_package.ps1` now only stage/remove `spark.dll` (the xaudio2
      detour is how spark itself gets loaded in production) — no more `smc64.dll`-specific
      logic. The end-user package this produces (`spark-$Config.zip`) is just the loader;
      a mod repo like `smc64` is expected to build its own package on top of this one
      (installing spark's package first/alongside, then dropping its own DLL into `mods/`).
- [x] `spark/shipfiles/SMC64_REAMDE.md` (Mario-specific end-user instructions) removed;
      replaced with a generic `spark/shipfiles/README.md` covering just the loader.
- [x] `ModLoader::loadAll()` now also scans directories listed in a `SPARK_MODS_PATH`
      environment variable (semicolon-delimited, like `PATH`), in addition to the default
      `<exe-dir>/mods/`, so a mod repo's own build output can be pointed at directly during
      dev without a copy step.
- [ ] Not yet done: verifying a full clean build of this repo standalone (no `smc64/`)
      with 0 errors, and updating `smc64` itself to add this repo as a submodule and wire
      its own build/package scripts on top.
