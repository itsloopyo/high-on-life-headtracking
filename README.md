# High On Life Head Tracking

![High On Life running with this mod](https://raw.githubusercontent.com/itsloopyo/high-on-life-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for High On Life that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - your head moves the view, your mouse or controller still controls the aim, and the crosshair stays on the point your shot will hit
- **6DOF tracking** - yaw, pitch and roll, plus positional lean, peek and duck
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- A copy of High On Life on [Steam](https://store.steampowered.com/app/1583230/) or on Xbox Game Pass. Two builds are supported: the Steam Win64 build of 2023-10-25, and the Xbox Game Pass WinGDK build of the same date (package 1.13.3652.0). The mod picks the right one by itself
- A tracking source: [OpenTrack](https://github.com/opentrack/opentrack) with a webcam, or any tracker that sends the OpenTrack UDP protocol. Not bundled
- 64-bit Windows 10 or 11

## Installation

### Lopari

Download [Lopari](https://lopari.app), choose **High On Life**, and click
**Play with head tracking**.

### Standalone Installer

1. Download the installer ZIP from the [releases page](https://github.com/itsloopyo/high-on-life-headtracking/releases).
2. Extract it anywhere outside the game folder.
3. Double-click `install.cmd`. It finds your copy of High On Life, on Steam or on Xbox Game Pass, deploys the mod and the Ultimate ASI Loader, and writes a state file so `uninstall.cmd` can remove exactly what it added.
4. Configure OpenTrack to output UDP to `127.0.0.1`, port `4242`.
5. Launch the game.

If the installer cannot find your game, point it at the folder yourself. Either set the environment variable `HIGH_ON_LIFE_PATH`, or pass the path as an argument. Give it the folder above `Oregon\`, not the folder holding the exe:

- Steam: `install.cmd "D:\SteamLibrary\steamapps\common\HighOnLife"`
- Xbox Game Pass: `install.cmd "D:\XboxGames\High on Life\Content"`

Success looks like `HeadTracking.ini` and `HeadTracking.log` appearing next to the game exe on first launch, with the log carrying a `build-check: matched profile ...` line naming your build, a `GetPlayerViewPoint hooked at RVA ...` line, and an `init complete.` line naming the port.

### Manual Installation

Mod managers do not deploy this mod. The payload has to sit next to the game exe, in `Oregon\Binaries\Win64\` on Steam or `Oregon\Binaries\WinGDK\` on Xbox Game Pass, and a mod manager deploys into one fixed subtree of the game folder that does not reach there. Vortex would report a successful install and nothing would load. There is no Nexus ZIP for that reason.

The installer ZIP holds `plugins\HighOnLifeHeadTracking.asi` and `vendor\ultimate-asi-loader\dinput8.dll`. Both go into the folder holding the game exe:

1. Copy `plugins\HighOnLifeHeadTracking.asi` into that folder.
2. Copy `vendor\ultimate-asi-loader\dinput8.dll` into that folder, then rename the copy to `winmm.dll`. The name matters: both shipping executables import `WINMM.dll` and neither imports `dinput8.dll` at all, so a loader left under the original name is never loaded.

That folder should then hold the game exe, `winmm.dll` and `HighOnLifeHeadTracking.asi` side by side. Nothing else in the ZIP belongs in the game folder.

On Xbox Game Pass the game exe itself cannot be opened or copied, which is normal and is not a permissions problem you need to fix. The folder it sits in still accepts new files, which is all the mod needs.

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

Each action's keys are a list in `HeadTracking.ini` (`ToggleKey`, `CycleTrackingModeKey`, `YawModeKey`), so either binding can be changed or removed there.

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

The tracking mode and the yaw mode you pick are saved to `HeadTracking.ini` as you change them, so the game starts in them next time. `End` changes the current session only: whether tracking is on when the game starts is `EnableOnStartup`.

### Aiming down sights

Head tracking stays on while you aim. The weapon stays where your mouse or
controller points it, so with your head turned it sits off to one side with its
sights still lined up, and your rounds land where those sights point. Head
movement is scaled to the zoom, so a scope does not magnify it.

Leaning eases out while the sights are up, because it would move your eye off them.

## Configuration

`HeadTracking.ini` is read once at startup, so a restart applies your edits, and deleting it gives you the defaults again at the next launch. A value the mod cannot read keeps its default, and `HeadTracking.log` names the line.

<!-- cameraunlock:config -->
The mod reads its settings from `HeadTracking.ini` in the game folder, at one of these paths depending on the store the game came from:

- `Oregon\Binaries\Win64\HeadTracking.ini`
- `Oregon\Binaries\WinGDK\HeadTracking.ini`

It creates the file when it starts and finds none. Edit it with any text editor.

Earlier versions of the mod used an older layout for this file. The first time this version starts, it converts the file once into the layout below and keeps the file as it was beside it as `HeadTracking.ini.pre-canonical`. `HeadTracking.ini.pre-canonical.last`, when present, is the file as it was before the most recent conversion: the mod converts the file again when it finds the older layout later, for example after an older version of the mod rewrote it.

Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:

- Reticle settings, and a key that toggled the reticle.
- A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.

An older version of the mod may not read the new layout correctly. It reads a key that moved as its own default, and it can misread a hotkey or another value that is now written as a name. To go back to an older version, first copy `HeadTracking.ini.pre-canonical` back over `HeadTracking.ini`, which restores the old file.

With every setting at its default, the file reads:

```ini
; High On Life head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=4242

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=true
; true: yaw turns around the world's up axis. false: around the camera's own up axis.
WorldSpaceYaw=true
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=true

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=0.0
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=0.15

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=true
; How far, in metres, leaning left or right can move the view.
PositionLimitX=0.3
; How far, in metres, raising your head can move the view.
PositionLimitY=0.2
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=0.2
; How far, in metres, leaning forward can move the view.
PositionLimitZ=0.4
; How far, in metres, leaning back can move the view.
PositionLimitZBack=0.1

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=End, Ctrl+Shift+Y
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=PageUp, Ctrl+Shift+G
; Switches yaw between the world's up axis and the camera's own (WorldSpaceYaw).
YawModeKey=PageDown, Ctrl+Shift+H

[Dev]
; true: log the distance to the point the crosshair is drawn from. The game's aim
; trace is working when that distance follows what the weapon points at.
AimProbe=false
```
<!-- /cameraunlock:config -->

## Troubleshooting

**Mod not loading**

- Check that `HeadTracking.log` exists next to the game exe (`Oregon-Win64-Shipping.exe` on Steam, `Oregon-WinGDK-Shipping.exe` on Xbox Game Pass). No log means the loader never loaded the mod: confirm `winmm.dll` is in that folder and is the file from `vendor\ultimate-asi-loader\`.
- If the log says your game build is newer, older or modified, the mod has no profile for it and stays dormant, so no hooks are installed and the game runs vanilla. Check the releases page for a build that covers your version.
- If you installed with a mod manager, that is the problem. See [Manual Installation](#manual-installation).

**No tracking response**

- Confirm OpenTrack's Output is `UDP over network` to `127.0.0.1` port `4242`, and that you pressed Start.
- Check `HeadTracking.log` for the `init complete.` line naming the port. If it says `waiting for the port to free up`, something else holds UDP 4242. Close it and tracking starts on its own within half a second.
- Press `End` (or `Ctrl+Shift+Y`) in case tracking was toggled off.

**Jittery or unstable tracking**

- If your tracker runs on this PC but sends to your LAN address rather than `127.0.0.1`, the mod classifies it as remote and applies `RemoteSmoothing`. Point the tracker at `127.0.0.1` to get `LocalSmoothing` instead.
- Raise `RemoteSmoothing` in `[Smoothing]` for a phone on WiFi, or route the phone through OpenTrack so its filters clean up the feed.
- Add filtering in your tracker. OpenTrack's accela filter and its curve mapping do this better than any setting here.

**The weapon is off to one side when I aim down sights**

- Your head is turned: the weapon stays on your aim and you are looking past it. Turn back to it, or move your aim to where you are looking.

**Yaw feels wrong when looking up or down at extreme angles**

- Toggle between world-locked and camera-local yaw with `Page Down` (or `Ctrl+Shift+H`). World-locked, the default, turns your head about the world up-axis and keeps the horizon level. Camera-local turns it about the camera's own up-axis, which leans the horizon on a pitched turn. The mode you pick is saved.
- If an axis moves the wrong way, invert it in your tracker, so every game behaves the same way.

## Updating

Download the new release and run `install.cmd` again. It overwrites the mod and the loader and leaves `HeadTracking.ini` alone, so your config is preserved.

## Uninstalling

Run `uninstall.cmd`. It removes the mod DLL, the state file and the mod's logs, and leaves `HeadTracking.ini` in place, with any `HeadTracking.ini.pre-canonical` and `HeadTracking.ini.pre-canonical.last` beside it, so your settings survive a reinstall. The Ultimate ASI Loader is only removed if the installer put it there; use `uninstall.cmd /force` to remove it anyway.

By hand: delete `HighOnLifeHeadTracking.asi`, `winmm.dll`, `HeadTracking.log` and `HeadTracking.prev.log` from the folder holding the game exe (`Oregon\Binaries\Win64\` on Steam, `Oregon\Binaries\WinGDK\` on Xbox Game Pass), and `HeadTracking.ini` and its `.pre-canonical` copies too if you do not want to keep your settings.

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
