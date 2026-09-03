# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo
#
# Check launcher-manifest.json against the rules the launcher enforces, so a
# broken contract fails here rather than on a player's machine. The two that
# actually bite are an absolute target and a `..` target: the launcher rejects
# both, and a manifest carrying one installs nothing with no useful message.

$ErrorActionPreference = 'Stop'

$projectDir = Split-Path -Parent $PSScriptRoot
$path = Join-Path $projectDir 'launcher-manifest.json'
if (-not (Test-Path $path)) { throw "launcher-manifest.json not found at $path" }

$manifest = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json

if ($manifest.schema_version -ne 2) {
    throw "schema_version is '$($manifest.schema_version)'; the fleet is on 2"
}
if ($manifest.delivery_mode -notin @('manifest', 'install_cmd', 'external')) {
    throw "delivery_mode is '$($manifest.delivery_mode)'; the deploy engine knows manifest, install_cmd and external"
}
if (-not $manifest.mod_info.game_id) {
    throw 'mod_info.game_id is missing'
}

# The game id has to exist in the catalog the installer resolves paths through.
# It not existing is what broke install.cmd and uninstall.cmd outright.
$games = Get-Content -LiteralPath (Join-Path $projectDir 'cameraunlock-core/data/games.json') -Raw | ConvertFrom-Json
if (-not $games.games.PSObject.Properties.Name.Contains($manifest.mod_info.game_id)) {
    throw "mod_info.game_id '$($manifest.mod_info.game_id)' has no entry in cameraunlock-core/data/games.json, so nothing can resolve the game folder"
}

foreach ($file in $manifest.files) {
    if ($file.target -match '^([A-Za-z]:|[\/])') {
        throw "manifest target is an absolute path, which the launcher rejects: $($file.target)"
    }
    if ($file.target -match '\.\.') {
        throw "manifest target escapes the game folder, which the launcher rejects: $($file.target)"
    }
}

Write-Host "launcher-manifest.json OK ($($manifest.files.Count) files, game_id $($manifest.mod_info.game_id))" -ForegroundColor Green
