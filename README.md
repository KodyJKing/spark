# Spark

A mod loader and SDK for Halo CE on the Master Chief Collection.

## Installation

Download the latest [release](https://github.com/KodyJKing/spark/releases), follow the
instructions in the archive's `README.md`, then drop whatever mod DLL(s) you want into the
`mods/` folder it creates.

## Developer Setup

Clone the repository recursively to get submodules:

```powershell
git clone --recursive
``` 

Install [Visual Studio 2022](https://visualstudio.microsoft.com/) and add MSBuild to your PATH. Location may vary. For me, it's located under `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin`. Alternatively, you can just install Visual Studio 2022 build tools.

Download [Premake](https://premake.github.io/) and add it to your path.

## Build Requirements

Spark **and every mod built for it** must link the CRT dynamically — `staticruntime "off"` in premake (`/MD` / `/MDd`). This is a hard requirement, not a preference:

- Spark hands out heap-owned objects (and shared-memory-referenced buffers) that must resolve to a single, process-wide CRT heap shared across `spark.dll` and every mod DLL.
- That shared heap is what lets allocations survive an uninject/re-inject cycle. With a statically-linked CRT, each DLL gets its own heap that is destroyed on unload, dangling any pointer handed across DLLs or persisted across injections.

All bundled premake projects already set this; keep it set in any new mod.

## Building and Running

```powershell
scripts/build.ps1
scripts/run_launcher.ps1
```

Halo MCC will need to be running when you run `run_launcher.ps1`. Note that without a mod DLL in `mods/`, spark on its own has no visible gameplay effect - see a mod repo like [smc64](https://github.com/KodyJKing/smc64) to develop against it.

## Workflow

### Injecting tags in Spool

1. Use **File > Open Map...** to open the host map.
2. Use **File > Open Guest Map for Injection...** to choose a second map.
   The picker is also available under **Tools > Tag Injection**.
3. Filter guest tags by group/path and check the tags to import. **Select filtered**
   selects all visible matches; selections survive filter changes. Dependencies
   are included automatically and matching host tags are reused.
4. Choose **Inject Selected and Save As...** and select an output `.map` file.

The default output name is `<host>.injected.map`. Spool refuses to overwrite
either input map, including alternate paths/hard links to those files. Other
existing outputs may be replaced after the native Save As confirmation; the
output is published only after a complete temporary file has been written.
Cancelling the dialog does not perform injection or write a file.

Injection uses raw input files rather than unsaved dissector edits. It is
experimental: unsupported schemas/resource formats are reported as errors, and
MCC acceptance is not guaranteed. See the
[Process C implementation notes](planning/tag-injection-process-c.md).

Guest bitmaps that reference shared external pixel data are permitted with a
warning. They require the same shared bitmap cache as the guest map; Spool does
not yet verify cache compatibility against mod-author checksums.

### Reusing structures in the dissector

Fields of type **Structure** or **BlockPointer** have a structure combo next to
the field-type picker, even when the block is empty or the field is collapsed.
Choose an existing structure by name; its ID is shown in muted text to
distinguish duplicate names. **Create new structure...** in the same combo
creates and assigns a fresh structure (4 bytes inline, 256 bytes for blocks).
The expanded structure still provides name and size editing. Reused structures
are shared, so editing their layout affects every field referencing them.
Click **Save** to persist assignments and edits.

### Certifying tag schemas

In a tag's dissector window, **Tag type certified complete** records that all
relocation-sensitive fields, including those in nested structures, have been
accounted for. It applies to every tag of that top-level type, not just the open
tag. Click **Save** to persist the flag in the schema's `complete` JSON property.
Older schemas without this property default to uncertified.

Uncertified types show warnings in the dissector and injection picker. Injection
also warns once per uncertified type actually processed, including host tags and
automatically imported dependencies, but is not blocked by certification alone.
Missing or invalid schemas still produce errors. Certification is a manual
assertion, not an automatic validation; uncheck it when edits invalidate it.

### Development

When developing, run `scripts/watch_launcher.ps1` to build/run in watch mode. When you save a source file, this script will uninject, rebuild, and reinject `spark.dll`.

If you're using Cheat Engine as part of your workflow, turn off symbols in the debug build configuration (`spark/premake5.lua`). Otherwise, Cheat Engine will hold on to the PDB file and prevent rebuilding.

## Packaging

If you want to package spark for distribution, you will need to build MSDetours' `setdll.exe` first. 

Make sure that `nmake` is on your path. For me, it is located under `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\<latest>\bin\Hostx64\x64`.

Then run

```powershell
.scripts/build_setdll.bat
```

To compile and generate the package, run
```powershell
./scripts/package.ps1 -Config Release
```
The generated `.zip` file will be under `bin/Release-Win64`.

## Scripts

All scripts assume you are running from the root of the repository.

### `build.ps1`

Builds spark once.

Arguments:

- `-Config` Configuration to build. Values are `Debug` and `Release`. Default is `Debug`.
- `-IDE` IDE premake will generate project files for. Default is `vs2022`.

### `run_launcher.ps1`

Injects spark into game once. MCC must already be running.

Arguments:

- `-Config` Configuration to run. Values are `Debug` and `Release`. Default is `Debug`.

- `-Arguments` Arguments to pass to the launcher executable. Optional.

### `watch_build.ps1`

Builds spark and recompiles on file change. 

Press `R` in the terminal to rebuild without waiting for a file change.

Arguments:

- Inherits from `build.ps1`.

### `watch_launcher.ps1`

Builds spark and runs launcher. Uninjects spark from game, recompiles and reruns launcher on file change. 

Press `R` in the terminal to rebuild without waiting for a file change.

Arguments:

- Inherits from `watch_build.ps1` and `run_launcher.ps1`.

### `package.ps1`

Packages mod into a `.zip` file. Creates a copy of xaudio2_9redist.dll that imports the mod.

You must manually run `build_setdll.bat` before running this script.

Arguments:

- Inherits from `build.ps1`.
- `-MCCPath` Path to the root of the MCC installation. This is where the xaudio dll is pulled from. Default is `C:\Program Files (x86)\Steam\steamapps\common\Halo The Master Chief Collection`.

### `install_package.ps1`

Runs `package.ps1` and installs the mod into the game. This script is for testing, not for end users.

Arguments:

- Inherits from `package.ps1`.
- `-MCCPath` Path to the root of the MCC installation. Default is `C:\Program Files (x86)\Steam\steamapps\common\Halo The Master Chief Collection`.
- `-Uninstall` Uninstalls the mod instead of installing it.

### `build_setdll.bat`

Builds `setdll.exe` (from MS Detours). This is only needed when packaging the mod for distribution.
