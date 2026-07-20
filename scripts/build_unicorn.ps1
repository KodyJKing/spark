# Builds vendor/unicorn (git submodule) via its CMake build, scoped to just
# the x86 backend as a static lib, in both Debug and Release configs so it
# can be linked directly into smc64-dlltest-unicorn's matching configs.
#
# Mirrors the external-dependency pattern used by build_libsm64.ps1: build
# via the vendor's own tooling (here, CMake) rather than trying to compile
# Unicorn's sources inline via premake globs.
#
# Usage:
#   .\scripts\build_unicorn.ps1            # configure (if needed) + build Debug & Release
#   .\scripts\build_unicorn.ps1 -Reconfigure  # force re-run cmake configure step

param(
    [switch]$Reconfigure
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$unicornSrc = Join-Path $root "vendor\unicorn"
$buildDir = Join-Path $unicornSrc "build"

if (-not (Test-Path (Join-Path $unicornSrc "CMakeLists.txt"))) {
    throw "vendor/unicorn is missing or the submodule hasn't been initialized. Run 'git submodule update --init --recursive'."
}

if ($Reconfigure -or -not (Test-Path (Join-Path $buildDir "unicorn.sln"))) {
    Write-Host "Configuring unicorn (x86 backend only, static lib, matching CRT per-config)..."
    cmake -S $unicornSrc -B $buildDir -G "Visual Studio 17 2022" -A x64 `
        -DUNICORN_ARCH=x86 `
        -DBUILD_SHARED_LIBS=OFF `
        -DUNICORN_BUILD_TESTS=OFF `
        -DUNICORN_INSTALL=OFF `
        -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded`$<`$<CONFIG:Debug>:Debug>DLL"
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
}

# We build the "unicorn_archive" target rather than plain "unicorn": Unicorn
# splits its build into several static libs (unicorn-static, unicorn-common,
# x86_64-softmmu, ...) and unicorn_archive is CMake's own bundling of all of
# them into a single combined unicorn.lib (see cmake/bundle_static.cmake),
# which is much simpler to link against from premake than tracking each
# sub-lib individually.
#
# NOTE: bundle_static_library's POST_BUILD step also tries to create a
# "unicorn.o" symlink next to unicorn.lib (for compatibility with other
# unicorn build front-ends we don't use). Creating symlinks requires a
# Windows privilege most dev machines don't have by default, so MSBuild
# reports this target as "FAILED" even though unicorn.lib itself was written
# successfully beforehand. We treat that specific failure as non-fatal and
# verify success by checking that unicorn.lib actually exists instead of
# trusting the exit code alone.
foreach ($config in @("Debug", "Release")) {
    Write-Host "Building unicorn ($config)..."
    cmake --build $buildDir --config $config --target unicorn_archive
    $libPath = Join-Path $buildDir "$config\unicorn.lib"
    if (-not (Test-Path $libPath)) {
        throw "cmake build failed for $config -- $libPath was not produced"
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  (ignoring MSBuild's nonzero exit -- it's the known-harmless unicorn.o symlink step; $libPath exists)"
    }
}

Write-Host ""
Write-Host "Done. unicorn.lib (bundled archive) built at:"
Write-Host "  $buildDir\Debug\unicorn.lib"
Write-Host "  $buildDir\Release\unicorn.lib"
