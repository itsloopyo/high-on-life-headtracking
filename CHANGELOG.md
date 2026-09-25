# Changelog

## [Unreleased]

### Added
- The tracking mode (`PageUp` / `Ctrl+Shift+G`) and the yaw mode (`PageDown` / `Ctrl+Shift+H`) are saved to `HeadTracking.ini` the moment you change them, so the game starts in them next time.

### Changed
- `HeadTracking.ini` has a new layout. The first time this version starts, it converts the file once into the new layout and keeps the file as it was beside it as `HeadTracking.ini.pre-canonical`. `HeadTracking.ini.pre-canonical.last`, when present, is the file as it was before the most recent conversion: the mod converts the file again when it finds the older layout later, for example after an older version of the mod rewrote it.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. `End`, `PageUp` and all three chords were fixed before and can now be changed or removed like any other key. `[Hotkeys] YawMode=0x22` becomes `YawModeKey=PageDown, Ctrl+Shift+H`.
- Settings keep their values under their new names: `[Network] Port` is `UdpPort`, `[Smoothing] Local` and `Remote` are `LocalSmoothing` and `RemoteSmoothing`, `[Position] LimitX` to `LimitZBack` are `PositionLimitX` to `PositionLimitZBack`, and `[Position] Enabled` is the startup tracking mode, written as `[General] RotationEnabled` and `[Position] PositionEnabled`.
- A value outside a setting's range is no longer clamped to the nearest end: it keeps the default, and `HeadTracking.log` names the line. `UdpPort` takes 1 to 65535, the smoothing values 0 to 1 and the position limits 0 to 10 metres.
- An older version of the mod may not read the new layout correctly. It reads a key that moved as its own default, and it can misread a hotkey or another value that is now written as a name. To go back to an older version, first copy `HeadTracking.ini.pre-canonical` back over `HeadTracking.ini`, which restores the old file.
- `uninstall.cmd` leaves `HeadTracking.ini` and its `.pre-canonical` copies in place, so your settings survive a reinstall.

### Removed
- The sensitivity and axis inversion settings (`[Sensitivity]`, `[Inversion]` and `[Position] SensitivityX/Y/Z`). Set these in your tracker app instead.
- With these settings at their shipped defaults the camera moves as it did before.
- The aim-down-sights mode cycle on `Insert` / `Ctrl+Shift+U` (b62aadc). `[View] AdsMode` and `[Hotkeys] AdsMode` are no longer read: head tracking stays on through the aim whatever the file says.

## [0.0.0] - 2026-09-01

### Added
- Initial release.
- Head tracking for the retro game at the start. That game is drawn by a scene capture onto a screen in the room rather than by the player's camera, so the head pose moves that capture and the room view around it is left clean. Its crosshair is a fixed mark the engine never projects, so the mod moves the game's own crosshair widget to follow the camera rather than drawing a second one over the top. Rotation only there: with the eye that draws and the eye that shoots at the same point, the mark is correct at every range.
- Head tracking for High On Life over the OpenTrack UDP protocol on port 4242, with rotation and position applied to the view while the mouse or controller keeps aiming.
- The crosshair marks where the shot lands, at any range and under any lean. The game's own crosshair is placed by the engine's matrices from the eye the frame is drawn from, so it follows the head; and the world point it is drawn from is the game's own aim trace rather than the fixed 40.96 m ray end the crosshair code asks for, so it stays on the impact point instead of agreeing at one distance and drifting either side of it.
- World-anchored HUD marks stay on the world under head movement: the info-scan ping's panels, mission waypoints, interaction prompts, dialogue speaker marks and compass markers are all placed by `UORWidget_HUDPrompt::UpdateLocation`, and its projection is now taken through the head-tracked view rather than the clean one.
- Aim-down-sights handling on `Insert` / `Ctrl+Shift+U`, cycling tracking paused and tracking on. There is no aim-marker mode: the game keeps its own crosshair through an aim, and it is already on the point the shot will hit.
- Hotkeys to toggle tracking (`End` / `Ctrl+Shift+Y`), cycle tracking mode (`Page Up` / `Ctrl+Shift+G`) and toggle world-locked or camera-local yaw (`Page Down` / `Ctrl+Shift+H`).
- `HeadTracking.ini`, written next to the game exe on first launch with comments on the settings that need them.
- Support for the PC Game Pass / Microsoft Store build alongside the Steam one. Both are recognised by fingerprint and each gets its own offsets, so one download covers either store, and the installer deploys to whichever copies are present.
- Build fingerprinting against a registry of known game builds, leaving the mod dormant on a build it does not recognise.
- `install.cmd` and `uninstall.cmd`, deploying the vendored Ultimate ASI Loader alongside the mod.
