# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

<#
.SYNOPSIS
    Copies the freshly built .asi and the vendored ASI loader into every local
    High On Life install.

.DESCRIPTION
    Steam and Game Pass can both be installed at once, and they are different
    binaries: Oregon\Binaries\Win64\Oregon-Win64-Shipping.exe against
    Oregon\Binaries\WinGDK\Oregon-WinGDK-Shipping.exe. Deploying to only the
    first one found is the quiet failure this iterates to avoid - you test a
    change, it does not appear, and the reason is that the copy you launched
    was never written to.

    Both builds statically import WINMM.dll, so the loader rename is the same
    for each.

.PARAMETER GamePath
    Deploy into this install root only, instead of every detected one.
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

# Which executable a given root holds depends on the store it came from, so the
# relpath is resolved per install rather than once.
function Get-InstallDescriptor {
    param([string]$Path)

    $isXbox = Test-IsXboxPath -Config $config -Path $Path
    $relpath = if ($isXbox -and $config.ContainsKey('XboxExecutable') -and $config.XboxExecutable) {
        $config.XboxExecutable
    } else {
        $config.Executable
    }
    return [pscustomobject]@{
        Label      = if ($isXbox) { 'Game Pass' } else { 'Steam' }
        Root       = $Path
        ExeRelpath = $relpath
    }
}

if ($GamePath) {
    if (-not (Test-Path -LiteralPath $GamePath -PathType Container)) {
        Write-Host "ERROR: supplied game path is not a directory: $GamePath" -ForegroundColor Red
        exit 1
    }
    $installs = @(Get-InstallDescriptor -Path $GamePath)
} else {
    $installs = @(Find-AllGamePaths -Config $config | ForEach-Object { Get-InstallDescriptor -Path $_ })
}

if ($installs.Count -eq 0) {
    Write-Host 'ERROR: no High On Life install found (Steam or Game Pass). Set HIGH_ON_LIFE_PATH or pass the path as the first argument.' -ForegroundColor Red
    exit 1
}

foreach ($install in $installs) {
    $exe = Join-Path $install.Root $install.ExeRelpath
    if (-not (Test-Path $exe)) {
        Write-Host "ERROR: game exe not found at $exe." -ForegroundColor Red
        exit 1
    }
    $exeDir = Split-Path $exe

    Write-Host ''
    Write-Host "=== $($install.Label): $exeDir ===" -ForegroundColor Cyan

    # ASI_LOADER_NAME in install.cmd is winmm.dll, which both the Win64 and the
    # WinGDK shipping exe statically import. The vendored artifact ships under
    # its upstream name and is renamed on the way in, the same rename the
    # installer performs.
    $loader = Join-Path $exeDir 'winmm.dll'
    if (-not (Test-Path $loader)) {
        Copy-Item (Join-Path $root 'vendor/ultimate-asi-loader/dinput8.dll') $loader -Force
        Write-Host '  Deployed Ultimate ASI Loader -> winmm.dll' -ForegroundColor Green
    }

    Copy-Item $asi (Join-Path $exeDir 'HighOnLifeHeadTracking.asi') -Force
    Write-Host '  Deployed HighOnLifeHeadTracking.asi' -ForegroundColor Green
}

Write-Host ''
Write-Host "Deployed to $($installs.Count) install(s)." -ForegroundColor Green
