# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

<#
.SYNOPSIS
    Skips the boot-time video playback in every local High On Life install so
    the dev loop for this mod is short.

.DESCRIPTION
    One change, reversible, and it touches no game file: a Game.ini is written
    into the game's own user config directory with an empty startup movie list,
    then marked read-only so the engine does not rewrite it on exit. The
    install's Oregon\Content\Movies is left alone - its videos are Bink .bk2,
    which we have no encoder for, so stubbing them is not an option here.

    Both stores write under %LOCALAPPDATA%\Oregon\Saved\Config, in a folder
    named for the build's platform: WindowsNoEditor for the Steam Win64 build,
    WinGDK for the Game Pass one. They are separate files, so a machine with
    both copies needs both written - applying to one and launching the other
    just plays the logos.

    A Game.ini the game wrote itself is moved aside as Game.ini.fastboot-backup
    and put back by -Action Restore.

.PARAMETER Action
    Apply (default), Restore, or Status.
#>
param(
    [ValidateSet('Apply', 'Restore', 'Status')]
    [string]$Action = 'Apply'
)

$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $root 'cameraunlock-core/powershell/GamePathDetection.psm1') -Force

$config = (Get-GameConfigs)['high-on-life']

$savedConfig = Join-Path $env:LOCALAPPDATA 'Oregon\Saved\Config'
$marker = '; managed by scripts/fast-boot.ps1 - delete this file to revert'

$gameIni = @"
$marker

[/Script/MoviePlayer.MoviePlayerSettings]
!StartupMovies=ClearArray
bWaitForMoviesToComplete=False
bMoviesAreSkippable=True
"@

# One target per installed store. Restore and Status also take the config
# folders that already exist, so a copy uninstalled after fast-boot was applied
# can still be put back rather than being silently skipped.
$platforms = [ordered]@{}
foreach ($path in (Find-AllGamePaths -Config $config)) {
    if (Test-IsXboxPath -Config $config -Path $path) {
        $platforms['WinGDK'] = 'Game Pass'
    } else {
        $platforms['WindowsNoEditor'] = 'Steam'
    }
}
if ($Action -ne 'Apply') {
    foreach ($name in @('WindowsNoEditor', 'WinGDK')) {
        if (-not $platforms.Contains($name) -and (Test-Path (Join-Path $savedConfig $name))) {
            $platforms[$name] = 'not installed'
        }
    }
}

if ($platforms.Count -eq 0) {
    Write-Host 'No High On Life install found (Steam or Game Pass), and no config folder to act on.' -ForegroundColor Yellow
    exit 1
}

foreach ($platform in $platforms.Keys) {
    $label = $platforms[$platform]
    $configDir = Join-Path $savedConfig $platform
    $gameIniPath = Join-Path $configDir 'Game.ini'
    $backupPath = "$gameIniPath.fastboot-backup"

    Write-Host ""
    Write-Host "=== $label ($platform) ===" -ForegroundColor Cyan

    switch ($Action) {

        'Apply' {
            New-Item -ItemType Directory -Force -Path $configDir | Out-Null

            if (Test-Path $gameIniPath) {
                Set-ItemProperty -Path $gameIniPath -Name IsReadOnly -Value $false
                if ((Get-Content -Path $gameIniPath -Raw) -notmatch [regex]::Escape($marker)) {
                    Move-Item -Force $gameIniPath $backupPath
                    Write-Host "  Kept the game's own Game.ini as Game.ini.fastboot-backup" -ForegroundColor Yellow
                }
            }

            Set-Content -Path $gameIniPath -Value $gameIni -Encoding utf8
            Set-ItemProperty -Path $gameIniPath -Name IsReadOnly -Value $true
            Write-Host "  Fast boot applied - wrote read-only $gameIniPath" -ForegroundColor Green
        }

        'Restore' {
            if (Test-Path $gameIniPath) {
                Set-ItemProperty -Path $gameIniPath -Name IsReadOnly -Value $false
                if ((Get-Content -Path $gameIniPath -Raw) -match [regex]::Escape($marker)) {
                    Remove-Item -Force $gameIniPath
                    Write-Host '  Removed the fast-boot Game.ini' -ForegroundColor Green
                }
            }
            if (Test-Path $backupPath) {
                Move-Item -Force $backupPath $gameIniPath
                Write-Host "  Restored the game's own Game.ini" -ForegroundColor Green
            }
            Write-Host '  Fast boot reverted.' -ForegroundColor Green
        }

        'Status' {
            $state = if (-not (Test-Path $gameIniPath)) {
                'absent'
            } elseif ((Get-Content -Path $gameIniPath -Raw) -match [regex]::Escape($marker)) {
                'fast-boot'
            } else {
                "the game's own"
            }
            Write-Host "  Game.ini in $configDir : $state" -ForegroundColor Cyan
            if (Test-Path $backupPath) {
                Write-Host '  Game.ini.fastboot-backup present' -ForegroundColor Cyan
            }
        }
    }
}
