# Builds and publishes the veil_world module to the Host PC's SpacetimeDB
# (MultiplayerSplitPlan.md W2, decision Q51: built and published by hand,
# outside the game's CMake; no CI until M-04).
#
#   powershell -ExecutionPolicy Bypass -File spacetime\publish_veil_world.ps1 [-Fresh]
#
# Needs spacetime\start_spacetime.ps1 running. Emscripten comes from the repo's
# own thirdparty\emsdk (activated here; no global install or PATH needed).
# -Fresh deletes the database's data first (every world).

param([switch]$Fresh, [string]$Database = "veil-world")
$ErrorActionPreference = "Stop"
$cli = Join-Path $env:LOCALAPPDATA "SpacetimeDB\bin\2.14.0\spacetimedb-cli.exe"
$module = Join-Path $PSScriptRoot "veil_world"

$emsdk = Join-Path $PSScriptRoot "..\thirdparty\emsdk" | Resolve-Path -ErrorAction SilentlyContinue
if (-not $emsdk -or -not (Test-Path (Join-Path $emsdk "emsdk_env.ps1"))) {
	Write-Error "emsdk not found at thirdparty\emsdk. It is committed to the repo (trimmed, Windows x64); restore it with: git checkout -- thirdparty/emsdk"
}
# Always the repo's own copy, never a global EMSDK or whatever emcc is on PATH.
$env:EMSDK = $emsdk.Path
$env:EMSDK_QUIET = "1"
& (Join-Path $emsdk "emsdk_env.ps1") | Out-Null
if (-not (Get-Command emcc -ErrorAction SilentlyContinue)) {
	Write-Error "emcc not found after activating $emsdk. The committed toolchain is incomplete; restore it with: git checkout -- thirdparty/emsdk"
}

& $cli server ping local | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Error "SpacetimeDB isn't answering on 127.0.0.1:3000. Run spacetime\start_spacetime.ps1 first." }

$publishArgs = @("publish", "--server", "local", "--module-path", $module, $Database, "--yes")
if ($Fresh) { $publishArgs += @("--delete-data=always") }
& $cli @publishArgs
exit $LASTEXITCODE
