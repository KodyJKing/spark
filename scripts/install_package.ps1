param (
    [string]$Config = "Debug",
    [string]$IDE = "vs2022",
    [string]$MCCPath = "C:\Program Files (x86)\Steam\steamapps\common\Halo The Master Chief Collection",
    [switch]$Uninstall = $false
)

if (Test-Path $MCCPath) {
    $MCCBinPath = "$MCCPath\mcc\binaries\win64"

    if (!$Uninstall) {
        # Package the project.
        & "./scripts/package.ps1" -Config $Config -IDE $IDE
        # Install the package.
        $ZipPath = "bin\$Config-Win64\spark-$Config.zip"
        # Unzip the package into the MCC directory.
        Expand-Archive -Path $ZipPath -Destination $MCCBinPath -Force
    } else {
        $XAudioBackupPath = "$MCCBinPath\xaudio2_9redist.dll~"
        if (!(Test-Path $XAudioBackupPath)) {
            Write-Host "Mod does not appear to be installed. Could not find xaudio2_9redist.dll backup at $XAudioBackupPath."
        } else {
            # Remove xaudio2_9redist.dll and spark.dll from the MCC directory.
            Remove-Item -Path "$MCCBinPath\xaudio2_9redist.dll" -Force
            Remove-Item -Path "$MCCBinPath\spark.dll" -Force
            # Restore xaudio backup.
            Rename-Item -Path $XAudioBackupPath -NewName "xaudio2_9redist.dll" -Force

            # Spark itself doesn't own any mod DLLs - leave mods/ (and its contents)
            # entirely alone here. Only clean it up if some other step already left it
            # empty; never touch files inside it, since those belong to individually
            # installed mods (e.g. smc64), not to spark.
            $ModsDir = "$MCCBinPath\mods"
            if ((Test-Path $ModsDir) -and !(Get-ChildItem -Path $ModsDir -Force)) {
                Remove-Item -Path $ModsDir -Force
            }
        }

    }

} else {
    Write-Host "Could not find the Halo The Master Chief Collection directory at $MCCPath."
}
