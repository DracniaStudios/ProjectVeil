# Builds and publishes the veil_world module to the Host PC's SpacetimeDB
# (MultiplayerSplitPlan.md W2, decision Q51: built and published by hand,
# outside the game's CMake; no CI until M-04).
#
#   powershell -ExecutionPolicy Bypass -File spacetime\publish_veil_world.ps1 [-Fresh]
#
# Needs spacetime\start_spacetime.ps1 running and Emscripten (emcc) on PATH or
# under $env:EMSDK. -Fresh deletes the database's data first (every world).

param([switch]$Fresh, [string]$Database = "veil-world")
$ErrorActionPreference = "Stop"
$cli = Join-Path $env:LOCALAPPDATA "SpacetimeDB\bin\2.14.0\spacetimedb-cli.exe"
$module = Join-Path $PSScriptRoot "veil_world"

if ($env:EMSDK -and (Test-Path (Join-Path $env:EMSDK "emsdk_env.ps1"))) {
	& (Join-Path $env:EMSDK "emsdk_env.ps1") | Out-Null
}
if (-not (Get-Command emcc -ErrorAction SilentlyContinue)) {
	Write-Error "emcc not found. Put Emscripten on PATH, or set EMSDK to your emsdk folder."
}

& $cli server ping local | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Error "SpacetimeDB isn't answering on 127.0.0.1:3000. Run spacetime\start_spacetime.ps1 first." }

$publishArgs = @("publish", "--server", "local", "--module-path", $module, $Database, "--yes")
if ($Fresh) { $publishArgs += @("--delete-data=always") }
& $cli @publishArgs
exit $LASTEXITCODE
