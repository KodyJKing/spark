param(
    [string]$Config = "Debug",
    [string]$IDE = "vs2022",
    [string]$Arguments = ""
)

$configArg = $Config.Replace("'", "''")
$ideArg = $IDE.Replace("'", "''")
$testArgs = $Arguments.Replace("'", "''")

& ".\scripts\watch.ps1" "." "& .\scripts\run_test.ps1 -Config '$configArg' -IDE '$ideArg' -Arguments '$testArgs'"
