param(
    [string]$Config = "Debug",
    [string]$IDE = "vs2022"
)

if ($IDE -ne "vs2022") {
    Write-Host "Warning: IDE $IDE is not standard. May not work as expected."
}

& "./scripts/kill_injected_instances.ps1"
& "./scripts/build_libsm64.ps1"
& "premake5.exe" $IDE
& "MSBuild.exe" "spark.sln" "/t:Build" "/p:Configuration=$Config" "/p:Platform=Win64"

if ($LASTEXITCODE -eq 0) {
    & "./scripts/copy_mods.ps1" -Config $Config
}

