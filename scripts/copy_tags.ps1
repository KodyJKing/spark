# Copy tags from the HCEEK home directory to the local project directory

$HceekHome  = if ($env:HCEEK_HOME) { $env:HCEEK_HOME } else { "C:\Program Files (x86)\Steam\steamapps\common\HCEEK" }
$ModTags = "/tags/smc64"
$RepoTags = "/tags"

$source = "$HceekHome$ModTags"
$destination = "$PSScriptRoot\..$RepoTags"

Copy-Item -Path "$source\*" -Destination $destination -Recurse -Force
