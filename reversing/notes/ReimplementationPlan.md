# Reimplementation Plan: Spark as a Partial Decompilation Project

Status: **planning only, no code changes yet.** Written 2026-07-20 after stepping
back to talk through the long-term ambition. This document is the anchor point
for that discussion — pick up here next session instead of re-deriving it.

## 1. The vision, restated

Reimplement portions of the Halo 1 engine in native C++, and keep a growing
test suite green as real engine calls are swapped for reimplemented ones.
Long-term, Spark becomes a **partial decompilation project**: not just a mod
that hooks the engine, but a project that can run mixed
real-engine/reimplemented-engine code, verified continuously, without
launching (and risking crashing) the live game for every check.

The user has flagged that this will likely **outgrow the current repo layout
and spin off into its own project**, with "radical restructuring" to make the
reimplement-verify-swap loop as smooth as possible. Nothing here should be
read as final — it's a starting map, not a commitment to a specific layout.

## 2. What already exists today (grounded inventory)

This is more built out than it first appeared. Three different *kinds* of
code are already mixed together under `smc64/src/engine/`, under one
`Engine::` namespace, without a naming convention distinguishing them:

1. **Genuine reimplementations** — real, from-scratch C++ logic with no
   dependency on live game memory. Example: [smc64/src/engine/math.cpp](smc64/src/engine/math.cpp)'s
   `inverseTransform`, `multiplyTransforms`, `orthonormalize` are hand-written
   math, not calls into halo1.dll. **These are already exactly the kind of
   code the decompilation-parity project needs — and as far as we know,
   none of them have ever been verified against the real engine
   function they mirror.** That's a gap and an opportunity (see §7).
2. **Live call-through wrappers** — resolve an address in the loaded
   halo1.dll and jump straight into the real function. Example:
   [smc64/src/engine/raycast.cpp](smc64/src/engine/raycast.cpp)'s `raycast()` just casts
   `dllBase() + 0xB913FC` to a function pointer and calls it. Not a
   reimplementation at all — a convenience wrapper around the original.
3. **Live memory accessors** — read/interpret live game state (entities, tags,
   BSP, datums, camera, player) without reimplementing any algorithm.
   Example: `entity/`, `tag.cpp`, `map.cpp`, `bsp/`, `datum/`.

None of this is currently distinguished by folder, namespace, or naming
convention — a reimplementation and a live-memory accessor both just look
like `Engine::something()`. That ambiguity will matter a lot once there's a
mix of "real" and "reimplemented" call paths that need to be told apart
programmatically (or at least trivially by a reader).

**Hook layer**: [smc64/src/spark/hook/HookTable.hpp](smc64/src/spark/hook/HookTable.hpp) is an X-macro list
binding named hooks (`UpdateCamera`, `DamageEntity`, `SpawnObject`, etc.) to
fixed offsets in halo1.dll, for intercepting engine calls from the live mod.
This is the mechanism that would eventually need to support "call the
reimplementation instead of the original" as a runtime-swappable choice,
separate from (but related to) the offline test harness.

**Test harnesses today** (both under the `smc64.sln` workspace, wired via the
root [premake5.lua](premake5.lua)):
- [smc64-dlltest](smc64-dlltest/premake5.lua) (`HALO1_TEST` macro, [TestRunner.hpp](smc64-dlltest/src/TestRunner.hpp)) —
  `LoadLibraryExA`'s the real halo1.dll and calls real functions directly, no
  emulation. Used for confirmed-pure functions. One test file today:
  `TransformTests.cpp` (validates `rotateVec`/`transformVec4AsPlane` — i.e.
  the *real* functions that `Engine::Transform`'s reimplemented methods
  mirror, but doesn't yet cross-check the reimplementation itself).
- [smc64-dlltest-unicorn](smc64-dlltest-unicorn/premake5.lua) (`UNICORN_TEST` macro,
  [TestRunner.hpp](smc64-dlltest-unicorn/src/TestRunner.hpp)) — emulates halo1.dll via Unicorn against a
  captured memory dump ([MinidumpLoader.hpp](smc64-dlltest-unicorn/src/MinidumpLoader.hpp)), for functions that
  depend on live entity/tag/BSP state. Five test files today, including the
  memory-access tracing diagnostic built 2026-07-20
  (`TraceMemoryAccessesTest.cpp`) that classifies whether a function's state
  dependencies would allow it to run without a dump at all.

**Important build detail discovered while grounding this plan**: both test
projects' premake files only glob-compile their **own** `src/**.cpp`; they
add `../smc64/src` purely as an `includedirs` entry. That means today, if a
test wanted to call `Engine::inverseTransform` (defined in
`smc64/src/engine/math.cpp`), the header declaration is visible but **the
function body is not compiled/linked into either test project** — there's no
existing path from a test to the reimplemented code's actual object file.
This is the first concrete wall the "radical restructuring" will need to
address (see §6).

**Struct-sharing convention**: [smc64/src/engine/types/](smc64/src/engine/types/) holds
plain-POD, header-only, `Engine::`-namespaced structs with
`static_assert(offsetof(...))` checks (e.g. `damage_event.hpp`'s
`Engine::DamageEvent`). Only 3 files exist there so far
(`damage_event.hpp`, `spawn_object_args.hpp`, `index.hpp`); most engine types
(`Transform`, `RaycastResult`, entity/tag structs) actually live directly in
their subject-matter headers (`math.hpp`, `raycast.hpp`, `entity/`, etc.)
rather than under `types/`. Worth reconciling once reimplementation work
needs a single shared struct definition used by *three* consumers (real-call
marshalling, reimplementation, and the live mod) instead of just one or two.

## 3. The core gap

There is currently **no test that calls both the real engine function and a
reimplementation with the same input and diffs the results.** Everything
built this session (the tracing/classification tool, the shared-dump
fixture, the two harnesses) produces reference results or classifies
promotability — none of it closes the loop by checking a reimplementation
against those reference results. That loop is the actual point of the
project as now described.

## 4. Proposed conceptual split (not yet implemented)

Separate what's currently flattened into one `Engine::` namespace into (at
least) two conceptual layers, whatever the eventual folder/project structure
turns out to be:

- **Engine Core** — pure reimplemented logic + shared POD types. No live
  game memory, no Spark/hook dependencies, no OS/game process assumptions.
  Portable enough to compile into: the live mod, `smc64-dlltest`,
  `smc64-dlltest-unicorn`, and (per §1) potentially a future standalone
  project. `inverseTransform`/`multiplyTransforms`/`orthonormalize` already
  qualify; more will move here as they're ported.
- **Engine Live Bindings** — call-through wrappers + live memory accessors
  (today's `raycast()`, `entity/`, `tag.cpp`, `map.cpp`, `bsp/`, `datum/`,
  `player.cpp`, `camera.cpp`). These stay coupled to a live/dumped process
  and are how both the mod and the test harnesses get *inputs* to feed into
  either the real function or Engine Core's reimplementation.

This split is exactly what makes a parity test expressible: build/derive an
input using Live Bindings (or a hand-authored literal), run it through (a)
the real function via whichever harness fits, and (b) the Engine Core
reimplementation, then diff.

## 5. Parity test harness — sketch

A new test category, distinct from `HALO1_TEST`/`UNICORN_TEST`, something
like:

```cpp
PARITY_TEST(InverseTransform_MatchesRealFunction) {
    Engine::Transform input = /* ... */;

    Engine::Transform real = callReal<Engine::Transform>(offset, input);   // via Halo1Module or Unicorn, whichever fits
    Engine::Transform mine = Engine::inverseTransform(input);              // Engine Core

    return diffFields(real, mine, /*epsilon*/ 1e-4f);  // rich per-field report on mismatch, not just true/false
}
```

Key design points to work out later, not now:
- **Rich diffs matter more than pass/fail** here — the whole point is
  telling you *which field* diverged and by how much while porting a
  function, not just red/green.
- **Float tolerance** needs a convention (engine likely uses standard IEEE
  float32 math, but instruction-level reordering/FMA could cause tiny
  divergence — needs an epsilon policy, probably per-field or per-type).
  Now that the plan exists we may want to *feed this back* into the tracing
  tool from earlier (already built) as a secondary sanity check: it can
  double as a check that the reimplementation touches the same (or a
  deliberately smaller) set of globals as the original, catching "forgot a
  dependency" bugs.
- **Where does `callReal<T>` dispatch to** — `smc64-dlltest`'s
  `Halo1Module` direct-call path for pure functions, or
  `smc64-dlltest-unicorn`'s dump+emulation path for stateful ones? Likely
  needs to be pluggable/parameterized rather than hardcoded per test, since
  the same reimplemented function might eventually need both (e.g. a
  currently-pure function might later be extended to touch state).

## 6. Restructuring path (staged — nothing here is urgent)

**Stage 0 (cheap, do first, low risk):** fix the build-wiring gap in §2 —
make Engine Core's `.cpp` files actually compile into both test projects
(and the mod). Likely as simple as adding explicit file globs, or (cleaner)
carving Engine Core out as its own premake project/static-lib that all three
consumers link against. This alone doesn't require moving any files across
repos, just fixing project wiring.

**Stage 1 (naming/location convention):** once there's more than
`math.cpp`'s handful of functions, decide where "Engine Core" reimplementations
physically live — a new top-level `smc64/src/engine-core/` (or similar)
directory, separate from the live-binding-heavy `engine/` folder, so the
distinction in §2 is visible at a glance instead of inferred by reading each
function body.

**Stage 2 (possible spinoff, later, bigger decision):** if/when Engine Core
grows enough to be useful independent of Spark/smc64 entirely (i.e. a
general-purpose "Halo 1 engine decompilation" library), consider extracting
it into its own repository, with `smc64` (the mod) and the test harnesses
as consumers/submodules rather than owners. This is explicitly a **later**
decision — don't restructure preemptively before there's enough reimplemented
surface area to know what the natural seams are.

## 7. Concrete first candidate

`Engine::inverseTransform`, `Engine::multiplyTransforms`, and
`Engine::orthonormalize` in [smc64/src/engine/math.cpp](smc64/src/engine/math.cpp) are
**already-written reimplementations with zero existing parity coverage**.
They're a good pilot because:
- They're pure (no live state), so `smc64-dlltest`'s existing
  `Halo1Module` direct-call path can supply the "real" side with no
  emulator involved at all.
- `TransformTests.cpp` already exists in that project and already validates
  the *real* `rotateVec`/`transformVec4AsPlane` — it's a short step from
  there to also validating the reimplemented inverse/multiply/orthonormalize
  against real-engine equivalents (once the real offsets for the matching
  operations are identified/confirmed in Ghidra — not yet done, would need a
  quick Static Analysis pass to find them if they aren't already documented).
- Low risk, high signal: a good way to prove out the whole parity-test
  pattern (harness shape, diff reporting, float tolerance policy) before
  investing in bigger restructuring.

## 8. Open questions (for next session, not urgent tonight)

- Does a real-engine equivalent of `inverseTransform`/`multiplyTransforms`/
  `orthonormalize` even exist as a single callable function, or is it inlined
  math scattered across call sites? Needs a Ghidra pass to confirm before
  committing to this as the pilot.
- What should the epsilon/tolerance convention be for float comparisons in
  parity tests?
- Should `PARITY_TEST` live in `smc64-dlltest`, `smc64-dlltest-unicorn`, or a
  new third project that depends on both (since a parity test may need
  either "real" backend depending on the function under test)?
- At what point does the live mod itself (via `HookTable.hpp`) want a
  runtime toggle to call the reimplementation instead of the original, vs.
  reimplementations only ever being verified offline in tests? (Not needed
  for the test-harness goal, but relevant to the "partial decompilation
  project" framing.)
- What's the actual scope of "portions of the engine" — is there a
  priority order (math/transforms → raycast/collision → entity update →
  rendering), or should that be decided function-by-function opportunistically?

## 9. Explicitly parked ideas (do not revisit without a fresh go-ahead)

- **"Guest tests"** (compiling test bodies to freestanding machine code
  running entirely inside Unicorn) — deferred 2026-07-20, documented in
  repo memory (`/memories/repo/smc64-conventions.md`).
- **"Commit portions of the dump"** (extracting small committed byte-range
  slices instead of the full `.DMP`, to make dump-backed tests
  portable/committable) — proposed and then explicitly shelved 2026-07-20
  ("I was hoping to avoid using the emulator in 'promoted' tests... let's
  hold off") after realizing it still requires the emulator and doesn't
  actually solve the stated goal.

## 10. Suggested next-session starting point

Don't start with restructuring. Start with §7: confirm (via Ghidra) whether
a real-engine equivalent of the transform math exists as an identifiable
function, then build one minimal `PARITY_TEST`-shaped harness (even
inline/ad-hoc, not the final macro design) to validate the *idea* end to
end before designing the general harness or touching project structure.
