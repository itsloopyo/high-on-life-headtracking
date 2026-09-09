# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

<#
.SYNOPSIS
    Launches High On Life for a head-tracking test run.

.DESCRIPTION
    Each store gets its own route, because neither one can start the other's
    copy. Steam goes through steam.exe -applaunch. Game Pass goes through the
    AppsFolder shell entry for the registered package, which is what the Xbox
    app itself invokes; running Oregon-WinGDK-Shipping.exe directly fails its
    licence check.

    The Oregon.exe at a Steam install root is a launcher shim; the shipping
    binary the mod hooks is Oregon\Binaries\Win64\Oregon-Win64-Shipping.exe,
    and Oregon\Binaries\WinGDK\Oregon-WinGDK-Shipping.exe on Game Pass.

.PARAMETER Store
    Which copy to start. Auto (default) picks the only installed one and
    refuses to guess when both are installed.

.PARAMETER Windowed
    Launch windowed at -ResX by -ResY instead of the saved display mode.
    Game Pass ignores this: the AppsFolder route takes no command line.
#>
param(
    [ValidateSet('Auto', 'Steam', 'GamePass')]
    [string]$Store = 'Auto',
    [switch]$Windowed,
    [int]$ResX = 1280,
    [int]$ResY = 720
)

$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $root 'cameraunlock-core/powershell/GamePathDetection.psm1') -Force

$config = (Get-GameConfigs)['high-on-life']

$installed = @{}
foreach ($path in (Find-AllGamePaths -Config $config)) {
    $key = if (Test-IsXboxPath -Config $config -Path $path) { 'GamePass' } else { 'Steam' }
    $installed[$key] = $path
}

if ($Store -eq 'Auto') {
    if ($installed.Count -eq 0) {
        throw 'No High On Life install found (Steam or Game Pass).'
    }
    if ($installed.Count -gt 1) {
        throw "Both stores are installed ($($installed.Keys -join ', ')). Pass -Store Steam or -Store GamePass."
    }
    $Store = @($installed.Keys)[0]
}

if (-not $installed.ContainsKey($Store)) {
    throw "No $Store install of High On Life found."
}

if ($Store -eq 'Steam') {
    $steam = Join-Path ${env:ProgramFiles(x86)} 'Steam\steam.exe'
    if (-not (Test-Path $steam)) {
        throw "steam.exe not found: $steam"
    }

    $gameArgs = @('-nosplash')
    if ($Windowed) { $gameArgs += @('-windowed', "-ResX=$ResX", "-ResY=$ResY") }

    Write-Host "Launching via Steam: $($gameArgs -join ' ')" -ForegroundColor Cyan
    Start-Process -FilePath $steam -ArgumentList (@('-applaunch', '1583230') + $gameArgs)
    return
}

# The package family name carries a per-publisher hash, so it is read from the
# registered package rather than written down. The application id is the one
# MicrosoftGame.config declares for the shipping executable.
$package = Get-AppxPackage -Name '2637SquanchGamesInc.HighonLife' | Select-Object -First 1
if (-not $package) {
    throw 'Game Pass install found on disk but the package is not registered for this user; start it once from the Xbox app.'
}

$target = "shell:AppsFolder\$($package.PackageFamilyName)!AppHighonLifeShipping"
if ($Windowed) {
    Write-Host 'Note: -Windowed is ignored for the Game Pass launch route, which takes no command line.' -ForegroundColor Yellow
}
Write-Host "Launching via Game Pass: $target" -ForegroundColor Cyan
Start-Process $target
