# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo
#
# Builds the release ZIP. One ZIP, not two: this mod loads through Ultimate
# ASI Loader, whose payload sits next to Oregon-Win64-Shipping.exe, and a mod
# manager deploys into one fixed subtree below the game folder. Nothing a
# manager installs reaches the exe directory, so there is no Nexus route and
# no -nexus.zip to build here. Do not add one back.
#
# The vendored loader is consumed exactly as committed under vendor/. Refreshing
# it is `pixi run update-deps`, a deliberate act with a commit attached, so
# packaging never reaches the network.

$ErrorActionPreference = 'Stop'

$projectDir = Split-Path -Parent $PSScriptRoot

Import-Module (Join-Path $projectDir 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force

$buildDir = Join-Path $projectDir 'build/Release'
$releaseDir = Join-Path $projectDir 'release'

$asi = Join-Path $buildDir 'HighOnLifeHeadTracking.asi'
if (-not (Test-Path $asi)) {
    throw "Built .asi not found at $asi. Run 'pixi run build' first."
}

# CMakeLists.txt is the canonical version; release.yml re-reads the same
# project() line to check the tag against the artifact it publishes.
$versionMatch = Select-String -Path (Join-Path $projectDir 'CMakeLists.txt') `
    -Pattern 'project\(HighOnLifeHeadTracking VERSION ([0-9]+\.[0-9]+\.[0-9]+)' | Select-Object -First 1
if (-not $versionMatch) { throw 'Could not read the project version from CMakeLists.txt' }
$version = $versionMatch.Matches[0].Groups[1].Value

if (Test-Path $releaseDir) { Remove-Item $releaseDir -Recurse -Force }
New-Item -ItemType Directory -Path $releaseDir | Out-Null

$stage = Join-Path $env:TEMP "hol-ht-stage-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $stage | Out-Null

# Mod payload. install.cmd's MOD_DLLS names this one file.
$plugins = New-Item -ItemType Directory -Path (Join-Path $stage 'plugins')
Copy-Item -Force $asi (Join-Path $plugins.FullName 'HighOnLifeHeadTracking.asi')
Write-Host '  plugins/HighOnLifeHeadTracking.asi' -ForegroundColor Green

# Vendored loader, verbatim. install.cmd looks for vendor\ultimate-asi-loader\
# next to itself and hard-errors without it, so a missing vendor directory is
# a broken ZIP, not a warning.
$vendorSrc = Join-Path $projectDir 'vendor/ultimate-asi-loader'
if (-not (Test-Path (Join-Path $vendorSrc 'dinput8.dll'))) {
    throw "vendor/ultimate-asi-loader/dinput8.dll not found. Run 'pixi run update-deps' and commit the result."
}
$vendorDst = New-Item -ItemType Directory -Path (Join-Path $stage 'vendor/ultimate-asi-loader') -Force
Copy-Item -Force -Recurse (Join-Path $vendorSrc '*') $vendorDst.FullName
Write-Host '  vendor/ultimate-asi-loader/' -ForegroundColor Green

# The launcher contract, stamped with the release version and staged at the ZIP
# root. The loader rename lives here as a `files` entry rather than as script
# logic, which is what keeps install.cmd and the launcher deploying the same
# thing. install.cmd's ASI_LOADER_NAME must stay in step with the target below.
$manifestSource = Join-Path $projectDir 'launcher-manifest.json'
if (-not (Test-Path $manifestSource)) {
    throw "launcher-manifest.json not found at $manifestSource - the launcher has no contract to read."
}
$manifest = Get-Content -LiteralPath $manifestSource -Raw | ConvertFrom-Json
$manifest.mod_info.version = $version
$manifest | ConvertTo-Json -Depth 10 | Set-Content -Path (Join-Path $stage 'launcher-manifest.json') -Encoding UTF8
Write-Host "  launcher-manifest.json (v$version)" -ForegroundColor Green

# Installer wrappers plus the shared bundle they call into. Copy-SharedBundle
# stages the whole set - the install/uninstall bodies, find-game.ps1 and the
# GamePathDetection module it imports, games.json - and asserts that
# THIRD-PARTY-NOTICES.md names the cameraunlock-core commit this build pins.
Copy-Item -Force (Join-Path $projectDir 'scripts/install.cmd') $stage
Copy-Item -Force (Join-Path $projectDir 'scripts/uninstall.cmd') $stage
Copy-SharedBundle -StagingDir $stage

# LICENSE, THIRD-PARTY-NOTICES.md and core's own licence are a distribution
# requirement, not documentation; README and CHANGELOG ride along.
Copy-LicenceNotices -StagingDir $stage -ProjectRoot $projectDir -Additional @('README.md', 'CHANGELOG.md')

$installerZip = Join-Path $releaseDir "HighOnLifeHeadTracking-v$version-installer.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $installerZip -Force
Write-Host "Built $installerZip" -ForegroundColor Green

# Left in place on a failure: a half-staged tree is the diagnostic.
Remove-Item $stage -Recurse -Force
