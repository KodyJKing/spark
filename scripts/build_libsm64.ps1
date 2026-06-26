Push-Location vendor\libsm64
.\scripts\make.bat
Pop-Location

# Convert MinGW DWARF debug info to a PDB so crash callstacks inside sm64.dll
# show real (internal) function names. Optional: skipped if cv2pdb isn't installed.
$cv2pdb = Get-Command cv2pdb64 -ErrorAction SilentlyContinue
if ($cv2pdb) {
    & $cv2pdb.Source vendor\libsm64\dist\sm64.dll
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "cv2pdb64 failed (exit $LASTEXITCODE); shipping DLL without a PDB."
    }
} else {
    Write-Warning "cv2pdb64 not found on PATH; sm64.pdb will not be generated."
}

$dest = if ($env:MCC_HOME) {
    "$env:MCC_HOME\MCC\Binaries\Win64"
} else {
    "C:\Program Files (x86)\Steam\steamapps\common\Halo The Master Chief Collection\MCC\Binaries\Win64"
}

Copy-Item -Force vendor\libsm64\dist\sm64.dll $dest
if (Test-Path vendor\libsm64\dist\sm64.pdb) {
    Copy-Item -Force vendor\libsm64\dist\sm64.pdb $dest
}
