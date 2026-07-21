# Memory Dumps for smc64-dlltest-unicorn

This directory holds captured memory-region "dumps" used by the Unicorn
emulation test suite. **Actual dump contents are gitignored** (`*.bin`,
manifest files, and any per-capture subdirectory) -- only this README is
committed. Dumps are large, machine/session-specific, and may contain
copyrighted game data, so they must never be committed.

## Why a separate suite

`smc64-dlltest` calls real halo1.dll functions directly (via `LoadLibraryExA`)
and only works for pure functions with no dependency on live game globals.
This suite instead loads a captured snapshot of relevant memory regions into
a Unicorn CPU emulator and calls into *that*, so it can exercise functions
that read/write global state without needing the actual game running.

## Manifest format

Each capture is a directory containing a manifest text file plus one `.bin`
file per captured region. See `DumpLoader.hpp` for the authoritative parser,
but in short:

```
# comment
<hex_base> <hex_or_dec_size> <perms:rwx, '-' for none> <bin_file|zero>
```

Example:

```
# halo1.dll .text (or whatever region a test needs)
0x00007ff9d8000000 0x00100000 r-x code.bin
# a thread's stack, captured live
0x0000004a12340000 0x00004000 rw- stack.bin
# scratch region -- content doesn't matter, just needs to be mapped
0x0000004a12350000 0x00001000 rw- zero
```

`<bin_file>` paths are relative to the manifest's own directory.

## Capturing a dump

Not yet automated. The plan is to use the Cheat Engine MCP tools
(`enum_memory_regions_full` to enumerate regions of interest, then
`read_region_from_file`/`read_memory` to pull their bytes) against the live
game process, write out the region bytes as `.bin` files, and hand-write (or
script) the accompanying manifest. This will likely grow into a small
capture script once the exact set of regions a given test needs is known.
