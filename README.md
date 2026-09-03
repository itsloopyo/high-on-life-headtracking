# High On Life Head Tracking

![High On Life running with this mod](https://raw.githubusercontent.com/itsloopyo/high-on-life-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for High On Life that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - your head moves the view, your mouse or controller still controls the aim, and the crosshair stays on the point your shot will hit
- **6DOF tracking** - yaw, pitch and roll, plus positional lean, peek and duck
- **Works with any OpenTrack compatible source** - webcam, phone app or VR headset, over UDP on port 4242

## Requirements

- A purchased copy of [High On Life](https://store.steampowered.com/app/1583230/) on Steam. The supported build is the Steam Win64 build of 2023-10-25
- A tracking source: [OpenTrack](https://github.com/opentrack/opentrack) with a webcam, or any tracker that sends the OpenTrack UDP protocol. Not bundled
- 64-bit Windows 10 or 11

## Installation

1. Download the installer ZIP from the [releases page](https://github.com/itsloopyo/high-on-life-headtracking/releases).
2. Extract it anywhere outside the game folder.
3. Double-click `install.cmd`. It finds your Steam copy of High On Life, deploys the mod and the Ultimate ASI Loader, and writes a state file so `uninstall.cmd` can remove exactly what it added.
4. Configure OpenTrack to output UDP to `127.0.0.1`, port `4242`.
5. Launch the game.

If the installer cannot find your game, point it at the folder yourself. Either set the environment variable `HIGH_ON_LIFE_PATH=D:\SteamLibrary\steamapps\common\HighOnLife`, or pass the path as an argument: `install.cmd "D:\SteamLibrary\steamapps\common\HighOnLife"`.

Success looks like `HeadTracking.ini` and `HeadTracking.log` appearing next to the game exe on first launch, with the log carrying `build-check: matched profile steam-win64-20231025`, a `GetPlayerViewPoint hooked at RVA ...` line, and an `init complete.` line naming the port.

### Manual Installation

Mod managers do not deploy this mod. The payload has to sit next to `Oregon-Win64-Shipping.exe` in `Oregon\Binaries\Win64\`, and a mod manager deploys into one fixed subtree of the game folder that does not reach there. Vortex would report a successful install and nothing would load. There is no Nexus ZIP for that reason.

The installer ZIP holds `plugins\HighOnLifeHeadTracking.asi` and `vendor\ultimate-asi-loader\dinput8.dll`. Both go into `Oregon\Binaries\Win64\`, next to the game exe:

1. Copy `plugins\HighOnLifeHeadTracking.asi` into that folder.
2. Copy `vendor\ultimate-asi-loader\dinput8.dll` into that folder, then rename the copy to `winmm.dll`. The name matters: `Oregon-Win64-Shipping.exe` imports `WINMM.dll` and does not import `dinput8.dll` at all, so a loader left under the original name is never loaded.

That folder should then hold `Oregon-Win64-Shipping.exe`, `winmm.dll` and `HighOnLifeHeadTracking.asi` side by side. Nothing else in the ZIP belongs in the game folder.

## Setting Up OpenTrack

- Input: whichever tracking source you have, see the subsections below
- Output: `UDP over network`, host `127.0.0.1`, port `4242`
- Map yaw, pitch and roll, plus X, Y and Z for positional tracking
- Press Start, then launch the game

Centring is done in your tracker: OpenTrack's Center bind, SteamVR's reset, or your phone app's CENTER button.

### VR Headset Setup

1. Connect the headset to your PC over Air Link, Virtual Desktop or a link cable.
2. Start SteamVR.
3. In OpenTrack, set **Input** to the SteamVR tracker.
4. Leave **Output** on `UDP over network`, host `127.0.0.1`, port `4242`.

### Webcam Setup

Set OpenTrack's **Input** to `neuralnet tracker`. It tracks your face from an ordinary webcam, with no markers, clips or IR hardware.

### Phone App Setup

A phone app is usable here if it sends the OpenTrack UDP protocol itself, or ships a PC-side companion that does. Plenty of phone trackers speak something else entirely, so check yours against that first.

For an app that does send it, what decides the wiring is how much filtering the app does before the packet leaves the phone. An app that filters on-device can point straight at this PC's LAN address (run `ipconfig` to find it) on UDP port `4242`. A raw or lightly filtered feed sent direct will jitter, because this mod's smoothing is sized to take the edge off a clean signal rather than to rescue a noisy one. That app should go through OpenTrack instead, so its filters and curves can clean the feed up first: send from the phone into OpenTrack on a spare port (`5252`, say, opened in your firewall), then out of OpenTrack to `127.0.0.1:4242`.

The test is quicker than the theory. Try direct, hold your head still, and if the view drifts or shakes, route it through OpenTrack.

I made [Headcam](https://headcam.app) so decent tracking was free for anybody with a phone already in their pocket. It filters on-device, so it can send direct. Any other app that filters enough works exactly the same way.

One thing to know about smoothing: a phone on WiFi is a remote connection and gets the `Remote` value, and so does a tracker on this same PC that sends to your LAN address instead of `127.0.0.1`. The mod picks between the two from the packet source address, so it sees a transport and not a machine.

## Controls

Two equivalent binding sets, use whichever your keyboard has:

| Action | Nav-cluster | Chord |
|--------|-------------|-------|
| Toggle tracking | `End` | `Ctrl+Shift+Y` |
| Cycle tracking mode | `Page Up` | `Ctrl+Shift+G` |
| Toggle yaw mode (world / camera-local) | `Page Down` | `Ctrl+Shift+H` |
| Cycle ADS mode | `Insert` | `Ctrl+Shift+U` |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

`Insert` / `Ctrl+Shift+U` cycles what happens when you aim down sights. Both
modes start the same way, raising the sights swings the view onto the point the
crosshair was marking so your shot lands where you had it lined up, and they
differ in what happens for the rest of the aim:

1. **Tracking paused** (default) - the game keeps the camera for as long as the
   sights are up. The sight picture is exactly the game's, and turning or leaning
   your head does nothing until you lower the weapon. Tilting it still rolls the
   view, in both modes: a tilt does not move your eye off the barrel or the aim
   off the middle of the frame, so there is nothing to hand back to the gun.
2. **Tracking on** - head tracking carries on from the snapped position, and the
   game's own crosshair stays on the point your shot will hit.

The choice is saved to `HeadTracking.ini`, so it survives a restart. Pressing the
key writes the mode you switched to into the mod's log.

## Configuration

`HeadTracking.ini` is written next to the game exe, in `Oregon\Binaries\Win64\`, on first launch. It is read once at startup, so a restart applies your edits, and deleting it resets everything to the defaults below.

```ini
; High On Life Head Tracking
;
; Centring is done in your tracker (OpenTrack's Center bind, SteamVR, or
; your phone app's CENTER button). The mod keeps no centre of its own.

[Network]
Port=4242

[General]
EnableOnStartup=true
; Yaw about the world up-axis (true) keeps the horizon level on a pitched
; turn; camera-local yaw (false) leans it. Toggled in game with Page Down
; or Ctrl+Shift+H; the toggle is not written back here.
WorldSpaceYaw=true

[Sensitivity]
Yaw=1.00
Pitch=1.00
Roll=1.00

[Inversion]
Yaw=false
Pitch=false
Roll=false

[Smoothing]
; Local applies to a tracker sending from this machine over loopback;
; Remote applies to anything else, including a phone on WiFi and this
; machine's own LAN address. Both cover rotation and position.
Local=0.00
Remote=0.15

[Position]
Enabled=true
SensitivityX=1.00
SensitivityY=1.00
SensitivityZ=1.00
; Metres. Z is asymmetric: more room to lean in than to pull back.
LimitX=0.30
LimitY=0.20
LimitYDown=0.20
LimitZ=0.40
LimitZBack=0.10

[View]
; What head tracking does while the sights are up. Cycled in game with
; Insert or Ctrl+Shift+U, and saved back here when you do.
;   paused   - tracking stands down for the aim (default, stock ADS)
;   tracked  - tracking carries on; the game's own crosshair stays on
;              the point your shot will hit
AdsMode=paused

[Hotkeys]
; Virtual-key codes. The Ctrl+Shift chords do the same jobs and are not
; configurable.
YawMode=0x22
AdsMode=0x2D

[Dev]
; Logs how far away the world point is that the crosshair is drawn from.
; A number that tracks whatever the weapon is pointed at is the aim
; trace working; a constant is it not. Off otherwise.
AimProbe=false
```

Sensitivities are accepted between 0.1 and 3.0, smoothing between 0.0 and 1.0, position sensitivities between 0.0 and 5.0, and the position limits between 0.01 and 0.5 metres. A value outside its accepted range is clamped and the substitution is written to `HeadTracking.log`, so the log says why a setting did not do what you expected.

## Troubleshooting

**Mod not loading**

- Check that `HeadTracking.log` exists next to `Oregon-Win64-Shipping.exe`. No log means the loader never loaded the mod: confirm `winmm.dll` is in that folder and is the file from `vendor\ultimate-asi-loader\`.
- If the log says your game build is newer, older or modified, the mod has no profile for it and stays dormant, so no hooks are installed and the game runs vanilla. Check the releases page for a build that covers your version.
- If you installed with a mod manager, that is the problem. See [Manual Installation](#manual-installation).

**No tracking response**

- Confirm OpenTrack's Output is `UDP over network` to `127.0.0.1` port `4242`, and that you pressed Start.
- Check `HeadTracking.log` for the `init complete.` line naming the port. If it says `waiting for the port to free up`, something else holds UDP 4242. Close it and tracking starts on its own within half a second.
- Press `End` (or `Ctrl+Shift+Y`) in case tracking was toggled off.

**Jittery or unstable tracking**

- If your tracker runs on this PC but sends to your LAN address rather than `127.0.0.1`, the mod classifies it as remote and applies the `Remote` smoothing value. Point the tracker at `127.0.0.1` to get the `Local` value instead.
- Raise `Remote` in `[Smoothing]` for a phone on WiFi, or route the phone through OpenTrack so its filters clean up the feed.
- Add filtering in your tracker. OpenTrack's accela filter and its curve mapping do this better than any setting here.

**Yaw feels wrong when looking up or down at extreme angles**

- Toggle between world-locked and camera-local yaw with `Page Down` (or `Ctrl+Shift+H`). World-locked, the default, turns your head about the world up-axis and keeps the horizon level. Camera-local turns it about the camera's own up-axis, which leans the horizon on a pitched turn.
- If an axis moves the wrong way, flip it in `[Inversion]`, or fix the axis mapping in OpenTrack so every game behaves the same way.

## Updating

Download the new release and run `install.cmd` again. It overwrites the mod and the loader and leaves `HeadTracking.ini` alone, so your config is preserved.

## Uninstalling

Run `uninstall.cmd`. It removes the mod DLL, the state file, and the mod's own `HeadTracking.ini` and logs. The Ultimate ASI Loader is only removed if the installer put it there; use `uninstall.cmd /force` to remove it anyway.

By hand: delete `HighOnLifeHeadTracking.asi`, `winmm.dll`, `HeadTracking.ini`, `HeadTracking.log` and `HeadTracking.prev.log` from `Oregon\Binaries\Win64\`.

## Building from Source

Needs Visual Studio 2022 with the C++ desktop workload, CMake 3.20 or newer, and [pixi](https://pixi.sh). No copy of the game is required.

```powershell
git clone --recurse-submodules https://github.com/itsloopyo/high-on-life-headtracking
cd high-on-life-headtracking
pixi run package
pixi run test
```

`pixi run package` produces `release/HighOnLifeHeadTracking-v<version>-installer.zip`. `pixi run test` runs the behaviour suites.

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details. Third-party components shipped beside or compiled into the `.asi` keep their own licences, reproduced in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Credits

- Squanch Games - developer and publisher of High On Life
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) - MIT
- [MinHook](https://github.com/TsudaKageyu/minhook) - BSD-2-Clause
- [cameraunlock-core](https://github.com/itsloopyo/cameraunlock-core) - MIT
- [OpenTrack](https://github.com/opentrack/opentrack) - ISC (wire protocol only, no code linked)

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Squanch Games. It requires a legitimately purchased copy of the game. Use at your own risk.
