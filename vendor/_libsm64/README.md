# libsm64 vendor files

Pre-built Windows import library and headers for libsm64.

## Files

| File | Description |
|---|---|
| `sm64.dll` | Built from the libsm64 source repo (`make` on Windows via MinGW) |
| `sm64.lib` | MSVC import library — lets MSVC/MSVC-linked projects link against the DLL |
| `sm64.def` | Module definition file listing all exported symbols |
| `sm64.exp` | Export file produced as a side effect of the `lib.exe` step below |
| `include/` | Public headers copied from `dist/include/` after the build |

## Regenerating sm64.lib

The Makefile in the libsm64 source repo produces `sm64.dll` via MinGW but does not
generate an MSVC-compatible import library. To produce one, run the following from
a **Visual Studio Developer Command Prompt** (x64) in this directory:

```cmd
lib.exe /def:sm64.def /machine:x64 /out:sm64.lib
```

This produces `sm64.lib` (the import library) and `sm64.exp` (intermediate export
file, can be ignored). Both are checked in so the smc64 build does not require a
VS prompt at configure time.

If the set of exported symbols in the DLL changes, update `sm64.def` to match
(add/remove lines under `EXPORTS`), then re-run the command above.
