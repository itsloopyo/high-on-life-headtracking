# Changelog

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
- Build fingerprinting against a registry of known game builds, leaving the mod dormant on a build it does not recognise.
- `install.cmd` and `uninstall.cmd`, deploying the vendored Ultimate ASI Loader alongside the mod.
