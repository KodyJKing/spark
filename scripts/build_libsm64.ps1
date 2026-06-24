Push-Location vendor\libsm64
.\scripts\make.bat
Pop-Location

$dest = if ($env:MCC_HOME) {
    "$env:MCC_HOME\MCC\Binaries\Win64"
} else {
    "C:\Program Files (x86)\Steam\steamapps\common\Halo The Master Chief Collection\MCC\Binaries\Win64"
}

Copy-Item -Force vendor\libsm64\dist\sm64.dll $dest
