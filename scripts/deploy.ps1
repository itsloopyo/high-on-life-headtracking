# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

<#
.SYNOPSIS
    Copies the freshly built .asi and the vendored ASI loader into a local
    High On Life install.

.PARAMETER GamePath
    Install root to deploy into. Omitted, the game is located the same way
    install.cmd locates it.
#>
[CmdletBinding()]
param([Parameter(Position = 0)][string]$GamePath)

$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $root 'cameraunlock-core/powershell/GamePathDetection.psm1') -Force

$asi = Join-Path $root 'build/Release/HighOnLifeHeadTracking.asi'
if (-not (Test-Path $asi)) {
    Write-Host "ERROR: build output not found at $asi. Run 'pixi run build' first." -ForegroundColor Red
    exit 1
}

# games.json is the only source of the game's location, exactly as it is for
# install.cmd. A literal copy here would be a second one, and the version of
# this script that carried one is why the missing catalog entry went unnoticed:
# the dev loop resolved the game and the shipped installer did not.
$gameId = 'high-on-life'
$catalog = Get-GameConfigs
if (-not $catalog.ContainsKey($gameId)) {
    Write-Host "ERROR: cameraunlock-core/data/games.json has no '$gameId' entry, so neither this script nor install.cmd can find the game. Update the submodule." -ForegroundColor Red
    exit 1
}
$config = $catalog[$gameId]

# A path passed on the command line wins over detection, exactly as it does
# for install.cmd's positional argument.
if ($GamePath) {
    if (-not (Test-Path -LiteralPath $GamePath -PathType Container)) {
        Write-Host "ERROR: supplied game path is not a directory: $GamePath" -ForegroundColor Red
        exit 1
    }
} else {
    $GamePath = Find-GamePath -Config $config
}
if (-not $GamePath) {
    Write-Host 'ERROR: High On Life install not found. Set HIGH_ON_LIFE_PATH or pass the path as the first argument.' -ForegroundColor Red
    exit 1
}

$exe = Join-Path $GamePath $config.Executable
if (-not (Test-Path $exe)) {
    Write-Host "ERROR: game exe not found at $exe." -ForegroundColor Red
    exit 1
}
$exeDir = Split-Path $exe

Write-Host "Deploying to $exeDir" -ForegroundColor Cyan

# ASI_LOADER_NAME in install.cmd is winmm.dll, which Oregon-Win64-Shipping.exe
# statically imports. The vendored artifact ships under its upstream name and
# is renamed on the way in, the same rename the installer performs.
$loader = Join-Path $exeDir 'winmm.dll'
if (-not (Test-Path $loader)) {
    Copy-Item (Join-Path $root 'vendor/ultimate-asi-loader/dinput8.dll') $loader -Force
    Write-Host '  Deployed Ultimate ASI Loader -> winmm.dll' -ForegroundColor Green
}

Copy-Item $asi (Join-Path $exeDir 'HighOnLifeHeadTracking.asi') -Force
Write-Host "Deployed HighOnLifeHeadTracking.asi to $exeDir" -ForegroundColor Green
