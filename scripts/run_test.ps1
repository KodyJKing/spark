param(
    [string]$Config = "Debug",
    [string]$IDE = "vs2022",
    [string]$Arguments = ""
)

& premake5.exe $IDE
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& MSBuild.exe spark.sln /t:spark-test "/p:Configuration=$Config" /p:Platform=Win64 /v:m /nologo
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& ([scriptblock]::Create("& `".\bin\$Config-Win64\spark-test\spark-test.exe`" $Arguments"))
exit $LASTEXITCODE
