@echo off
:: SPDX-License-Identifier: MIT
:: Copyright (c) 2026 itsloopyo
:: ============================================
:: High On Life Head Tracking - Install
:: ============================================
:: Thin wrapper - install body lives in cameraunlock-core/scripts/install-body-asi.cmd,
:: staged into the release ZIP's shared/ by Copy-SharedBundle. To change
:: install behaviour edit the body, not this wrapper.
::
:: Source of truth for everything below the CONFIG BLOCK:
:: cameraunlock-core/scripts/templates/install-wrapper-asi.cmd. Copy this
:: file to <mod>/scripts/install.cmd, fill in the CONFIG BLOCK, change nothing
:: else. scripts/conformance.ps1 checks that nothing else changed.
::
:: Ultimate ASI Loader: one DLL renamed to the proxy the game already
:: imports, with the mod shipped as an .asi beside the game exe. Check the
:: exe's import table before choosing ASI_LOADER_NAME - a proxy the game does
:: not import is never loaded and the mod silently does nothing.
:: ============================================

:: --- CONFIG BLOCK ---
set "GAME_ID=high-on-life"
set "MOD_DISPLAY_NAME=High On Life Head Tracking"
set "MOD_DLLS=HighOnLifeHeadTracking.asi"
set "MOD_INTERNAL_NAME=HighOnLifeHeadTracking"
set "MOD_VERSION=0.0.0"
set "STATE_FILE=.headtracking-state.json"
set "FRAMEWORK_TYPE=ASILoader"
:: Filename the ASI loader DLL is renamed to: the import the game exe already
:: has. winmm.dll, dinput8.dll, dxgi.dll and xinput1_3.dll are the common ones.
:: Oregon-Win64-Shipping.exe statically imports WINMM.dll; dinput8.dll is not
:: in its import table at all, so a loader named dinput8.dll is never loaded.
set "ASI_LOADER_NAME=winmm.dll"
:: Subdirectory below the exe directory to deploy into, for engines that load
:: their proxy DLL from somewhere other than beside the exe. Source engine wants
:: bin\; a copy next to the exe is never loaded. Leave empty for everything
:: else, and set the same value in uninstall.cmd.
set "ASI_SUBDIR="
:: Files copied only when they are not already there, so an upgrade keeps
:: whatever the user tuned. Listing an .ini in MOD_DLLS instead puts it through
:: the unconditional copy and resets every key on every update.
:: Empty: the mod writes its own default HeadTracking.ini at first run
:: (config_write_default_if_missing), so there is no shipped file to seed.
set "MOD_SEED_FILES="
:: Version of the vendored Ultimate ASI Loader, recorded in the state file so
:: the launcher can tell which loader build it is looking at. Leave empty to
:: omit the field. Bump alongside vendor/ via `pixi run update-deps`.
set "ASI_LOADER_VERSION=9.7.4"
:: Post-install help text. `&echo ` starts each further line.
set "MOD_CONTROLS=Controls:&echo   End / Ctrl+Shift+Y      - Toggle head tracking on/off&echo   Page Up / Ctrl+Shift+G  - Cycle tracking mode (full / rotation only / position only)&echo   Page Down / Ctrl+Shift+H - Toggle yaw mode (world-locked / camera-local)&echo   Insert / Ctrl+Shift+U   - Cycle ADS mode (paused / marker / tracked)"
:: --- END CONFIG BLOCK ---

:: Pin delayed expansion off before `%*` is expanded on the `call` below.
:: Under `cmd /V:ON`, or with DelayedExpansion=1 in
:: HKCU\Software\Microsoft\Command Processor, cmd.exe eats a `!` out of the
:: expanded line, and a real game path like C:\Games\Oh! My Game reaches the
:: body already mangled. The body pins expansion off at its own outer scope
:: too, but that is one `call` too late to save the argument it was handed.
setlocal disabledelayedexpansion

set "WRAPPER_DIR=%~dp0"
set "_BODY=%WRAPPER_DIR%shared\install-body-asi.cmd"
if not exist "%_BODY%" set "_BODY=%WRAPPER_DIR%..\cameraunlock-core\scripts\install-body-asi.cmd"
if not exist "%_BODY%" (
    echo ERROR: install-body-asi.cmd not found in shared\ or ..\cameraunlock-core\scripts\.
    echo If this is a release ZIP, re-download it from GitHub ^(corrupt installer^).
    echo If this is the dev tree, run: git submodule update --init --recursive
    exit /b 1
)
call "%_BODY%" %*
exit /b %errorlevel%
