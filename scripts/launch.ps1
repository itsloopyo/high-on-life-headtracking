# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

<#
.SYNOPSIS
    Launches High On Life for a head-tracking test run.

.DESCRIPTION
    Goes through steam.exe -applaunch so the game starts under Steam the way a
    player starts it. The Oregon.exe at the install root is a launcher shim;
    the shipping binary the mod hooks is
    Oregon\Binaries\Win64\Oregon-Win64-Shipping.exe.

.PARAMETER Windowed
    Launch windowed at -ResX by -ResY instead of the saved display mode.
#>
param(
    [switch]$Windowed,
    [int]$ResX = 1280,
    [int]$ResY = 720
)

$ErrorActionPreference = 'Stop'

$steam = Join-Path ${env:ProgramFiles(x86)} 'Steam\steam.exe'
if (-not (Test-Path $steam)) {
    throw "steam.exe not found: $steam"
}

$gameArgs = @('-nosplash')
if ($Windowed) { $gameArgs += @('-windowed', "-ResX=$ResX", "-ResY=$ResY") }

Write-Host "Launching via Steam: $($gameArgs -join ' ')" -ForegroundColor Cyan
Start-Process -FilePath $steam -ArgumentList (@('-applaunch', '1583230') + $gameArgs)
