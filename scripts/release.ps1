# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo

[CmdletBinding()]
param(
    # NOT Mandatory: PowerShell satisfies a missing mandatory parameter by
    # reading stdin, and `pixi run release` allocates no TTY - that prompt dies
    # with "IOException: The handle is invalid" instead of printing a usage
    # line. Validate it ourselves and fail fast.
    [Parameter(Position = 0)][string]$Version,
    # Ship a release even when there are no user-facing commits since the last
    # tag (writes a maintenance changelog entry instead of aborting).
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..')

if (-not $Version) {
    Write-Host 'Usage: pixi run release <major|minor|patch|nightly|X.Y.Z> [-Force]' -ForegroundColor Red
    exit 1
}

if ($Version -eq 'nightly') {
    & (Join-Path $PSScriptRoot 'release-nightly.ps1')
    exit $LASTEXITCODE
}

Import-Module (Join-Path $root 'cameraunlock-core/powershell/ReleaseWorkflow.psm1') -Force

# THIRD-PARTY-NOTICES.md names the cameraunlock-core commit compiled into the
# release ZIP, and bumping the submodule does not touch it. Packaging refuses to
# ship that mismatch, so a bump with no notices edit stops the release here
# rather than in CI once the tag has already been pushed.
& git -C $root diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) { throw 'THIRD-PARTY-NOTICES.md has uncommitted edits. Commit or discard them, then re-run.' }
& (Join-Path $root 'cameraunlock-core\scripts\sync-core-notices.ps1') -Repo $root
if ($LASTEXITCODE -ne 0) { throw "sync-core-notices.ps1 exited $LASTEXITCODE - fix THIRD-PARTY-NOTICES.md before releasing." }
& git -C $root diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) {
    & git -C $root commit -q -m 'chore: record the cameraunlock-core commit this build compiles' -- THIRD-PARTY-NOTICES.md
    if ($LASTEXITCODE -ne 0) { throw 'Could not commit the re-synced THIRD-PARTY-NOTICES.md.' }
    Write-Host 'THIRD-PARTY-NOTICES.md re-synced to the pinned cameraunlock-core commit.' -ForegroundColor Yellow
}

# Windows PowerShell 5.1's `-Encoding utf8` means UTF-8 WITH a BOM, and pixi
# rejects a pixi.toml that starts with one ("Missing table in manifest"). The
# release then aborts inside `pixi run package`, after the version has already
# been stamped into several files and before the tag exists - a half-bumped tree
# with no way forward. Write raw text through .NET instead, which also leaves
# each file's existing line endings alone (Get-Content/Set-Content round-trips
# every file to CRLF, and install.cmd must stay CRLF while the rest must not
# silently become it).
function Set-TextFileNoBom {
    param([string]$Path, [string]$Text)
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding $false))
}

function Update-VersionInFile {
    param([string]$Path, [string]$Pattern, [string]$Replacement)
    $full = Join-Path $root $Path
    $text = [System.IO.File]::ReadAllText($full)
    $updated = $text -replace $Pattern, $Replacement
    if ($updated -eq $text) { throw "Version stamp did not match anything in $Path" }
    Set-TextFileNoBom -Path $full -Text $updated
}

# Mirrors New-ChangelogFromCommits' insertion so a -Force maintenance entry
# lands in the same place with the same shape.
function Add-MaintenanceChangelogEntry {
    param([string]$Path, [string]$NewVersion)
    $date = Get-Date -Format 'yyyy-MM-dd'
    $entry = "## [$NewVersion] - $date`n`n### Changed`n`n- Maintenance release (no user-facing changes).`n`n"
    $changelog = Get-Content (Join-Path $root $Path) -Raw
    if ($changelog -match '(?s)(# Changelog.*?)(## \[)') {
        $changelog = $changelog -replace '(?s)(# Changelog.*?\n\n)', "`$1$entry"
    } else {
        $changelog = $changelog -replace '(?s)(# Changelog.*?\n)', "`$1$entry"
    }
    $changelog = $changelog.TrimEnd() + "`n"
    Set-TextFileNoBom -Path (Join-Path $root $Path) -Text $changelog
}

Push-Location $root
try {
    # CMakeLists.txt is canonical: release.yml re-reads this exact pattern to
    # check the tag against the built artifact.
    $verLine = Select-String -Path 'CMakeLists.txt' -Pattern 'project\(HighOnLifeHeadTracking VERSION ([0-9]+\.[0-9]+\.[0-9]+)'
    if (-not $verLine) { throw 'Could not read the project version from CMakeLists.txt' }
    $current = $verLine.Matches[0].Groups[1].Value
    $new = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $current
    if (-not (Test-SemanticVersion -Version $new)) { throw "Resolved version '$new' is not MAJOR.MINOR.PATCH." }

    # New-ReleaseTag pushes to `main`, so releasing from any other branch would
    # push commits the branch does not contain. Gate before anything mutates.
    $branch = (git rev-parse --abbrev-ref HEAD).Trim()
    if ($branch -ne 'main') { throw "Releases cut from 'main' only; currently on '$branch'." }
    if (-not (Test-CleanGitStatus)) { throw 'Working tree is dirty - commit or stash first.' }
    if (Test-GitTagExists -Tag "v$new") { throw "Tag v$new already exists." }

    # Generate CHANGELOG from commits since last tag. This is the gate that
    # aborts when there are no user-facing commits, so run it BEFORE mutating
    # any version files or building - a failure here then leaves a clean tree
    # instead of stranding a half-applied version bump with no tag.
    try {
        New-ChangelogFromCommits -ChangelogPath 'CHANGELOG.md' -Version $new | Out-Null
    } catch {
        if (-not $Force) {
            Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
            Write-Host 'No user-facing changes to release. Re-run with -Force for a maintenance release.' -ForegroundColor Yellow
            exit 1
        }
        Write-Host 'No user-facing commits since last tag - writing maintenance entry (-Force).' -ForegroundColor Yellow
        Add-MaintenanceChangelogEntry -Path 'CHANGELOG.md' -NewVersion $new
    }

    # Stamp the new version into every place it lives. CMakeLists.txt is
    # canonical; the other two are hand-kept copies and drift silently if
    # skipped (pixi.toml is the workspace version, install.cmd prints
    # MOD_VERSION to the user and records it in the state file).
    $versionFiles = @('CMakeLists.txt', 'pixi.toml', 'scripts/install.cmd', 'launcher-manifest.json', 'CHANGELOG.md')
    Update-VersionInFile -Path 'CMakeLists.txt' -Pattern 'project\(HighOnLifeHeadTracking VERSION [0-9.]+' -Replacement "project(HighOnLifeHeadTracking VERSION $new"
    Update-VersionInFile -Path 'pixi.toml' -Pattern '(?m)^version = "[0-9.]+"' -Replacement "version = `"$new`""
    Update-VersionInFile -Path 'scripts/install.cmd' -Pattern '(?m)^set "MOD_VERSION=[0-9.]+"' -Replacement "set `"MOD_VERSION=$new`""
    # The packager stamps the staged copy too, but the committed one is what the
    # tag check in release.yml reads, so it has to move with the others.
    Update-VersionInFile -Path 'launcher-manifest.json' -Pattern '"version": "[0-9.]+"' -Replacement "`"version`": `"$new`""

    # Build and package through the same pixi chain CI runs (setup -> build ->
    # package), not a bare `cmake --build`, which fails outright on a checkout
    # where build/ was never configured.
    & pixi run package
    if ($LASTEXITCODE -ne 0) { throw 'Build/packaging failed' }

    # Not Invoke-VersionCommit: it hardcodes "chore: bump version to X", and
    # build.yml skips the redundant build job on a head commit starting with
    # "Release v". A mismatched subject there costs a full duplicate CI build
    # of the tag that release.yml is already building.
    foreach ($f in $versionFiles) {
        git add -- $f
        if ($LASTEXITCODE -ne 0) { throw "git add failed for $f" }
    }
    if (-not (git diff --cached --name-only)) { throw 'Version stamping produced no staged changes.' }
    git commit -m "Release v$new"
    if ($LASTEXITCODE -ne 0) { throw 'Failed to commit the release' }

    New-ReleaseTag -Version $new -Message "Release v$new"
    Write-Host "Released v$new" -ForegroundColor Green
} finally {
    Pop-Location
}
