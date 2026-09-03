# Ultimate ASI Loader (vendored)

Bundled copy of Ultimate ASI Loader, the install-time source of truth.
Refresh manually with `pixi run update-deps`, then commit.

## Snapshot

- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader
- Tag: `v9.7.4`
- Commit: `6b440669144c4a0bef5718ab155df160d231cd42`
- Asset: `Ultimate-ASI-Loader_x64.zip`
- Upstream URL: https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.4/Ultimate-ASI-Loader_x64.zip
- dinput8.dll SHA-256: `fa266e3513d02c08a1b808f28c10538a489eaffaa4b0707f7cc1066e71b5afd7`
- Fetched at: 2026-09-01T11:50:41.5765447+01:00

`dinput8.dll` is extracted from the upstream asset untouched. install.cmd copies it to
`<game>/Oregon/Binaries/Win64/winmm.dll` as the ASI hook slot (Oregon-Win64-Shipping.exe
statically imports WINMM.dll and does not import dinput8.dll at all).
