# Changelog

## [Unreleased]

### Added
- The tracking mode (`PageUp` / `Ctrl+Shift+G`) and the yaw mode (`PageDown` / `Ctrl+Shift+H`) are saved to `CameraUnlock.ini` the moment you change them, so the game starts in them next time.
- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.

### Changed
- Settings move to `Oregon\Binaries\Win64\CameraUnlock.ini` (Steam) or `Oregon\Binaries\WinGDK\CameraUnlock.ini` (Xbox Game Pass). Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. `End`, `PageUp` and all three chords were fixed before and can now be changed or removed like any other key. `[Hotkeys] YawMode=0x22` becomes `YawModeKey=PageDown, Ctrl+Shift+H`.
- Settings keep their values under their new names: `[Network] Port` is `UdpPort`, `[Smoothing] Local` and `Remote` are `LocalSmoothing` and `RemoteSmoothing`, `[Position] LimitX` to `LimitZBack` are `PositionLimitX` to `PositionLimitZBack`, and `[Position] Enabled` is the startup tracking mode, written as `[General] RotationEnabled` and `[Position] PositionEnabled`.
- A value in `CameraUnlock.ini` outside a setting's range is not clamped to the nearest end: it keeps the default, and `HeadTracking.log` names the line. `UdpPort` takes 1 to 65535, the smoothing values 0 to 1 and the position limits 0 to 10 metres. A value `HeadTracking.ini` held is imported as the earlier versions read it.
- `uninstall.cmd` leaves `CameraUnlock.ini` and `HeadTracking.ini` in place, so your settings survive a reinstall.

### Removed
- The sensitivity, scale, deadzone, response curve and axis inversion settings (`[Sensitivity]`, `[Inversion]` and `[Position] SensitivityX/Y/Z`). Set these in your tracker app instead.
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
