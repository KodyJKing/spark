param(
    [string]$Config = "Debug",
    [string]$IDE = "vs2022",
    [string]$Arguments = ""
)

$build = "premake5.exe $IDE; MSBuild.exe spark.sln /t:spool /p:Configuration=$Config /p:Platform=Win64 /v:m /nologo"
$run = "& ./bin/$Config-Win64/spool/spool.exe $Arguments"

& "./scripts/watch.ps1" "." "$build; if (`$LASTEXITCODE -eq 0) { $run }"
