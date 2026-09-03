# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

[CmdletBinding()]
param(
    [switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $ProjectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force

$cmakeLists = Join-Path $ProjectRoot 'CMakeLists.txt'
$versionMatch = Select-String -Path $cmakeLists -Pattern 'project\(HighOnLifeHeadTracking VERSION ([0-9]+\.[0-9]+\.[0-9]+)'
if (-not $versionMatch) {
    throw "Could not extract version from $cmakeLists"
}
$version = $versionMatch.Matches[0].Groups[1].Value

# Two deviations from the module defaults: this mod's payload lands next to the
# game exe, so the packager builds no Nexus ZIP (see package-release.ps1), and
# pixi.toml names the Release build task `build`, not `build-release`.
Publish-NightlyBuild `
    -ModId 'high-on-life' `
    -ModName 'HighOnLifeHeadTracking' `
    -Version $version `
    -ProjectRoot $ProjectRoot `
    -BuildCommand 'pixi run build' `
    -NoNexusZip `
    -AllowDirty:$AllowDirty
