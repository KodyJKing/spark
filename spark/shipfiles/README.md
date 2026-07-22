## Install

Paste the contents of this archive under `...\Halo The Master Chief Collection\mcc\binaries\win64`. Allow file replacement if prompted.

Drop any mod DLLs you want loaded into the `mods\` folder here (created alongside `spark.dll`).

## Uninstall

Delete `spark.dll`, then rename `xaudio2_9redist.dll~` back to `xaudio2_9redist.dll`.

## Usage

Start Halo MCC *without* Easy Anti-Cheat. Spark loads every `*.dll` in `mods\` (plus any
directories listed in the `SPARK_MODS_PATH` environment variable) on startup.
