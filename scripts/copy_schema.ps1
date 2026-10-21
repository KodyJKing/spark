# Copy the injected instance's tag schema from the MCC install into the repo.
param(
    [string]$MCCPath = "C:\Program Files (x86)\Steam\steamapps\common\Halo The Master Chief Collection"
)

$source = "$MCCPath\MCC\Binaries\Win64\tags.schema2.json"
$destination = "$PSScriptRoot\..\spark\shipfiles\tags.schema2.json"

if (-not (Test-Path $source)) {
    Write-Host "Schema not found at $source, skipping."
    exit 0
}

Copy-Item -Path $source -Destination $destination -Force
