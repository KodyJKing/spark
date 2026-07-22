# Spark

A mod loader for Halo CE on the Master Chief Collection.

Spark itself doesn't contain any gameplay mod - it hooks the engine, patches
`xaudio2_9redist.dll` to load `spark.dll` in production, and loads every `*.dll` it finds in a
`mods/` folder (plus any directories listed in the `SPARK_MODS_PATH` environment variable),
calling each one's exported `spark_modLoad()` entry point. Mods (e.g.
[smc64](https://github.com/KodyJKing/smc64)) live in their own repositories and depend on this
one as a git submodule for the engine API (`SparkAPI.h`), hook registration, event buses, and
`Engine::` query functions.

## Installation

Download the latest [release](https://github.com/KodyJKing/spark/releases), follow the
instructions in the archive's `README.md`, then drop whatever mod DLL(s) you want into the
`mods/` folder it creates.

## Developer Setup

Clone the repository recursively to get submodules:

```powershell
git clone --recursive
``` 

Install [Visual Studio 2022](https://visualstudio.microsoft.com/) and add MSBuild to your PATH. Location may vary. For me, it's located under `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin`.

## Building and Running

```powershell
scripts/build.ps1
scripts/run_launcher.ps1
```

Halo MCC will need to be running when you run `run_launcher.ps1`. Note that without a mod DLL in `mods/`, spark on its own has no visible gameplay effect - see a mod repo like [smc64](https://github.com/KodyJKing/smc64) to develop against it.

## Workflow

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
