param(
    [string]$Message
)

$root   = Split-Path $PSScriptRoot -Parent
$source = Join-Path $root "reversing\ghidra"

$resticArgs = @("backup", $source)

if ($Message) {
    $resticArgs += @("--tag", $Message)
}

& restic @resticArgs
