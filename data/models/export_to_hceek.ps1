# Copy /models/* to <HCEEK_HOME>/data/smc64/*
# JMS files should go under <HCEEK_HOME>/data/smc64/<model>/models/*
# TIFF files should go under <HCEEK_HOME>/data/smc64/<model>/bitmaps/*

$ModelsDir  = "$PSScriptRoot"
$HceekHome  = if ($env:HCEEK_HOME) { $env:HCEEK_HOME } else { "C:\Program Files (x86)\Steam\steamapps\common\HCEEK" }
$Smc64Data  = "$HceekHome\data\smc64"

foreach ($modelDir in Get-ChildItem -Path $ModelsDir -Directory) {
    $name = $modelDir.Name

    # JMS files -> <HCEEK>/data/smc64/<model>/models/
    $jmsFiles = Get-ChildItem -Path $modelDir.FullName -Filter "*.JMS" -File
    if ($jmsFiles) {
        $destModels = "$Smc64Data\$name\models"
        New-Item -ItemType Directory -Path $destModels -Force | Out-Null
        foreach ($f in $jmsFiles) {
            Copy-Item -Path $f.FullName -Destination "$destModels\$($f.Name)" -Force
            Write-Host "Copied JMS: $($f.Name) -> $destModels"
        }
    }

    # TIFF files -> <HCEEK>/data/smc64/<model>/bitmaps/
    $tiffFiles = Get-ChildItem -Path "$($modelDir.FullName)\bitmaps" -Filter "*.tiff" -File -ErrorAction SilentlyContinue
    if ($tiffFiles) {
        $destBitmaps = "$Smc64Data\$name\bitmaps"
        New-Item -ItemType Directory -Path $destBitmaps -Force | Out-Null
        foreach ($f in $tiffFiles) {
            Copy-Item -Path $f.FullName -Destination "$destBitmaps\$($f.Name)" -Force
            Write-Host "Copied TIFF: $($f.Name) -> $destBitmaps"
        }
    }
}

