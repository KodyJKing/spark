# dlmalloc allocator

The engine's heap is a statically-linked build of Doug Lea's `dlmalloc` (public-domain,
matches the 2.8.x line closely). This mirrors the `dlMalloc`/`dlFree` convention already
documented throughout the Xbox 360 `halocea` decompile (`apDLALLOC_IFACE` etc.) — the PC/MCC
build (`halo1.dll`, x64) links the same allocator family.

## How it was identified

Started from a stub named `FUN_7ffec78cc760___mem_util_function`. Its decompilation showed:

- `mem2chunk`-style pointer adjustment (`mem - 0x10`) and a bounds check against a global
  (`least_addr`).
- `PINUSE`/`CINUSE` bit tests on a size-with-flags header field.
- Backward/forward chunk consolidation, unlinking from smallbins (`size >> 3` index into a
  32-bit bitmap) and treebins (classic `compute_tree_index` bit-twiddling), and top/designated
  victim (`dv`) special cases.
- A call to release memory back to the OS that turned out to be a byte-for-byte match of
  dlmalloc's `win32munmap` (loop of `VirtualQuery` + `VirtualFree(..., MEM_RELEASE)`,
  checking `MEM_COMMIT` and matching `BaseAddress`/`AllocationBase`).

That is enough surface area to be confident this is `dlfree`. From there, sibling/xref
functions were checked and matched cleanly against the known dlmalloc source layout
(smallbin/treebin carving, expand-into-top realloc, `sys_trim`'s segment walk, etc.).

## Function map (high confidence — near-exact structural match to public dlmalloc.c)

| Address | Name | Notes |
|---|---|---|
| `dlfree` | `dlFree` | was `FUN_...___mem_util_function` |
| `dlmalloc` | `dlmalloc` | core allocator; was mislabeled `__allocArray` (low-confidence guess from an earlier session) |
| `dlmemalign` | `dlmemalign` | was `dlmalloc` briefly (see correction below); alignment<=8 tail-calls `dlmalloc`, else over-allocates and frees head/tail remainders via `dlFree` |
| `dlrealloc` | `dlrealloc` | shrink-in-place / expand-into-top / alloc+memcpy+free fallback |
| `win32munmap` | `win32munmap` | `VirtualQuery`/`VirtualFree` loop, called by `dlFree` and `sys_trim` |
| `sys_trim` | `sys_trim` | releases trailing OS pages from the top chunk |
| `init_top` | `init_top` | called from `sys_trim` after shrinking the top chunk |
| `release_unused_segments` | `release_unused_segments` | called at the end of `sys_trim` |
| `tmalloc_small` | `tmalloc_small` | treebin allocation path, small request |
| `tmalloc_large` | `tmalloc_large` | treebin allocation path, large request |
| `sys_alloc` | `sys_alloc` | grows the heap (new segment) when top can't satisfy a request |

**Correction note:** `dlmemalign` (`0x7ffec78cbe00`) was briefly, incorrectly renamed
`dlmalloc` before its `CMP RDX,8 / JA` alignment check and tail-call into the real
`dlmalloc` (`0x7ffec78cc3c0`) were noticed. Fixed in the same session.

## `mstate` (global heap state) — high confidence

All of `dlfree`/`dlmalloc`/`dlrealloc`/`sys_trim` reference a single static `mstate` (this
build is NOT compiled with `ONLY_MSPACES` — there's one global heap). The base of the struct
is `0x7ffeca5164e0`. Field offsets below were cross-checked against **two independent call
sites** (the "expand into top" branch of `dlrealloc`, and `sys_trim`'s footprint/topsize
usage), so the mapping to canonical `struct malloc_state` is considered solid:

| Offset | Global | Field | Evidence |
|---|---|---|---|
| +0x00 | `DAT_dlmalloc_smallmap` | `smallmap` | 32-bit bitmap, `BTR`/`BTS` by `size>>3` index in `dlFree`/`dlmalloc` |
| +0x04 | `DAT_dlmalloc_treemap` | `treemap` | 32-bit bitmap, `BT`/`BTS` by tree index |
| +0x08 | `DAT_dlmalloc_dvsize` | `dvsize` | paired writes with `dv` throughout `dlFree`/`dlmalloc` |
| +0x10 | `DAT_dlmalloc_topsize` | `topsize` | confirmed via `dlrealloc`'s expand-into-top branch and `sys_trim` |
| +0x18 | `DAT_dlmalloc_least_addr` | `least_addr` | lower-bound sanity check used everywhere (`ok_address`) |
| +0x20 | `DAT_dlmalloc_dv` | `dv` | designated victim chunk pointer |
| +0x28 | `DAT_dlmalloc_top` | `top` | confirmed via `dlrealloc`'s expand-into-top branch (`next == top`) |
| +0x30 | `DAT_dlmalloc_trim_check` | `trim_check` | threshold compared against `topsize` before calling `sys_trim` |
| +0x350 | `DAT_dlmalloc_footprint` | `footprint` | decremented by `psize` after a successful `win32munmap` in `dlFree`, and in `sys_trim` |

Not yet confirmed in the binary (asserted only by analogy to the well-known public-domain
dlmalloc source, since these are fixed by the library's constants rather than
compiler-specific): `smallbins[66]` at some offset after `top`/`trim_check`/`release_checks`/
`magic`, then `treebins[32]`, then `footprint`/`max_footprint`/`mflags`/segment info. **Do not
treat these as verified** — only the header fields in the table above have been
cross-referenced against real disassembly.

`DAT_dlmalloc_granularity` (`0x7ffeca516878`) is a separate global (not part of `mstate`) —
matches dlmalloc's `mparams.granularity`, used in `sys_trim`'s unit-alignment math and in
`dlrealloc`'s split-shrink heuristic. Medium-high confidence.

A `malloc_state` struct (category `/dlmalloc`, size `0x358`) now exists in Ghidra's data type
manager with the verified fields above plus an explicit `pad` field (`0x40`-`0x350`) covering
the unverified bins/segment region, so it isn't mistaken for confirmed layout. It has been
applied at `0x7ffeca5164e0` (had to clear the individually auto-defined `DAT_dlmalloc_*`
primitives first — Ghidra won't overlay a struct on already-defined data). `dlFree`/`dlmalloc`/
`dlrealloc`/`sys_trim` now decompile as `DAT_dlmalloc_smallmap.<field>` accesses instead of
flat globals. The base symbol is still named `DAT_dlmalloc_smallmap` (its offset-0 field is
also `smallmap`, which reads a bit oddly) — consider renaming the base label to something like
`g_dlmalloc_state`.

## Suggested next steps

- Rename the base global (`DAT_dlmalloc_smallmap`, i.e. the `malloc_state` instance at
  `0x7ffeca5164e0`) to `g_dlmalloc_state` so it doesn't read as `g_dlmalloc_state.smallmap`
  ambiguously.
- Narrow down the unverified bins/segment region (currently one opaque `pad` field) if it
  becomes relevant to a hook (mostly useful if we ever want to walk live bins in Cheat Engine).
- A Cheat Engine script that reads `DAT_dlmalloc_footprint`/`topsize` live would be a cheap way
  to sanity check the struct against a running process (footprint should track total committed
  heap bytes).
- `sys_alloc`, `tmalloc_small`, `tmalloc_large`, `init_top`, `release_unused_segments` are named
  by strong contextual match but haven't been decompiled line-by-line against upstream source
  the way `dlfree`/`dlmalloc`/`dlrealloc`/`sys_trim` were — worth a follow-up pass if any of
  them become relevant to a hook.
