# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo
#
# THIRD-PARTY-NOTICES.md is the attribution that travels inside the release ZIP,
# so it has to name the loader version actually vendored. vendor/<slug>/README.md
# is the authority: update-deps.ps1 writes it from the upstream release it just
# fetched. The two drifted apart once already (notices said 9.7.3 while 9.7.4
# shipped), which is why this is a task rather than a habit.

$ErrorActionPreference = 'Stop'

$projectDir = Split-Path -Parent $PSScriptRoot

$vendorReadme = Join-Path $projectDir 'vendor/ultimate-asi-loader/README.md'
$tagMatch = Select-String -Path $vendorReadme -Pattern '^- Tag: `([^`]+)`' | Select-Object -First 1
if (-not $tagMatch) { throw "No '- Tag:' line in $vendorReadme" }
$vendored = $tagMatch.Matches[0].Groups[1].Value

$notices = Join-Path $projectDir 'THIRD-PARTY-NOTICES.md'
$noticeMatch = Select-String -Path $notices -Pattern '^- \*\*Version:\*\* (v[0-9.]+)' | Select-Object -First 1
if (-not $noticeMatch) { throw "No Ultimate ASI Loader version line in $notices" }
$declared = $noticeMatch.Matches[0].Groups[1].Value

if ($vendored -ne $declared) {
    throw "THIRD-PARTY-NOTICES.md declares $declared but vendor/ultimate-asi-loader ships $vendored. Run 'pixi run update-deps', or correct the notices."
}

Write-Host "THIRD-PARTY-NOTICES.md matches the vendored loader ($vendored)" -ForegroundColor Green
