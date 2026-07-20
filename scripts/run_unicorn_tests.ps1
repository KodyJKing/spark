# Builds and runs the smc64-dlltest-unicorn test suite (Unicorn-emulator-
# based tests that exercise halo1.dll code/data against a captured
# minidump -- see smc64-dlltest-unicorn/dumps/README.md).
#
# Usage:
#   .\scripts\run_unicorn_tests.ps1              # build (Debug) + run everything
#   .\scripts\run_unicorn_tests.ps1 -Config Release
#   .\scripts\run_unicorn_tests.ps1 -Clean       # force a clean rebuild first
#   .\scripts\run_unicorn_tests.ps1 -Filter LineTest
#   .\scripts\run_unicorn_tests.ps1 -Filter LineTest,RotateVec  # OR'd together
#
# -Filter takes one or more case-insensitive substrings matched against test
# names; only matching tests run (others are counted as "skipped by
# filter", not failed). Still builds the whole suite either way -- this
# only skips *running* old tests you don't need to re-verify right now.
#
# NOTE on -Clean: MSBuild's fast-up-to-date check has repeatedly been
# observed to report "All outputs are up-to-date" for this project even
# after editing a header included by one of its .cpp files, or after
# premake regenerated the .vcxproj for a newly-added source file -- so
# -Clean force-deletes the obj dir first whenever you're not 100% sure the
# last build actually picked up your changes.

param(
    [string]$Config = "Debug",
    [switch]$Clean,
    [string[]]$Filter = @()
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $unicornLib = Join-Path $root "vendor\unicorn\build\$Config\unicorn.lib"
    if (-not (Test-Path $unicornLib)) {
        throw "vendor/unicorn hasn't been built for $Config yet ($unicornLib not found). Run .\scripts\build_unicorn.ps1 first."
    }

    if ($Clean) {
        $objDir = Join-Path $root "obj\$Config-Win64\smc64-dlltest-unicorn"
        Write-Host "Removing $objDir for a clean rebuild..."
        Remove-Item $objDir -Recurse -Force -ErrorAction SilentlyContinue
    }

    Write-Host "Regenerating project files (premake5 vs2022)..."
    & "premake5.exe" "vs2022"
    if ($LASTEXITCODE -ne 0) { throw "premake5 failed" }

    Write-Host "Building smc64-dlltest-unicorn ($Config)..."
    & "MSBuild.exe" "smc64.sln" "/t:smc64-dlltest-unicorn" "/p:Configuration=$Config" "/p:Platform=Win64" "/m"
    if ($LASTEXITCODE -ne 0) { throw "MSBuild failed" }

    $exe = Join-Path $root "bin\$Config-Win64\smc64-dlltest-unicorn\smc64-dlltest-unicorn.exe"
    if (-not (Test-Path $exe)) {
        throw "Build reported success but $exe was not produced."
    }

    if ($Filter.Count -gt 0) {
        Write-Host "Running $exe (filter: $($Filter -join ', '))..."
    } else {
        Write-Host "Running $exe..."
    }
    & $exe @Filter
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
