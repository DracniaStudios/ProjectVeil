# Starts the Host PC's SpacetimeDB for Veil (MultiplayerSplitPlan.md W1, decision Q44).
#
# Run it once, before Start Server, and leave its window open:
#   powershell -ExecutionPolicy Bypass -File spacetime\start_spacetime.ps1
#
# - Pinned to CLI 2.14.0 by calling that version's binary directly, so the
#   global `spacetime version use` default (other projects) is left alone.
#   Install it once with:  spacetime version install 2.14.0 --yes
# - Listens on 127.0.0.1 only. The default is 0.0.0.0:3000, which would put the
#   World store on the network (review B3). Only the Host's own server talks to it.
# - Keeps Veil's data in its own directory, apart from any other local database.

$ErrorActionPreference = "Stop"
$version = "2.14.0"
$cli = Join-Path $env:LOCALAPPDATA "SpacetimeDB\bin\$version\spacetimedb-cli.exe"
$dataDir = Join-Path $env:LOCALAPPDATA "SpacetimeDB\veil-data"

if (-not (Test-Path $cli)) {
	Write-Error "SpacetimeDB CLI $version is not installed. Run: spacetime version install $version --yes"
}

New-Item -ItemType Directory -Force $dataDir | Out-Null
Write-Host "Starting SpacetimeDB $version on 127.0.0.1:3000 (data: $dataDir)"
& $cli start --listen-addr 127.0.0.1:3000 --data-dir $dataDir
