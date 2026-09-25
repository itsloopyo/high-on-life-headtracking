// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <string>

#include <windows.h>

#include "logging.h"

#include "cameraunlock/config/ini_reader.h"

namespace hol_ht::config {

namespace {

constexpr const char* kIniName = "HeadTracking.ini";

// The accepted range of every value the INI can carry. These are the fleet-wide
// bounds from AGENTS.md, named here so the clamp below reads as what it enforces
// rather than as a wall of literals.
constexpr int   kMinPort = 1024;
constexpr int   kMaxPort = 65535;
constexpr float kMinSensitivity = 0.1f;
constexpr float kMaxSensitivity = 3.0f;
constexpr float kMinSmoothing = 0.0f;
constexpr float kMaxSmoothing = 1.0f;
constexpr float kMinPositionSensitivity = 0.0f;
constexpr float kMaxPositionSensitivity = 5.0f;
constexpr float kMinPositionLimitM = 0.01f;
constexpr float kMaxPositionLimitM = 0.5f;
// Virtual-key codes run 0x01 to 0xFE. Anything else is not a key, so the
// binding would poll a code no keyboard produces and the action would quietly
// have no hotkey at all.
constexpr int kMinVirtualKey = 0x01;
constexpr int kMaxVirtualKey = 0xFE;

// Held from Load() so SaveAdsMode() can write back to the same file the player
// edited, rather than guessing at the game directory a second time.
std::string g_iniPath;

std::string IniPath(const std::string& exe_dir) {
    return exe_dir + "\\" + kIniName;
}

// A float key's bad value is silent in two different ways, and neither of them
// is caught by a clamp written as two comparisons. strtod accepts "nan", and a
// NaN fails BOTH comparisons, so it passes through untouched and reaches the
// sensitivity, the composed FRotator and the frame the player is looking at. An
// infinity does not pass through, but what it does instead is no better: it
// clamps to the end of the range, so `LimitZ=inf` reads back as the widest
// travel the mod allows rather than as the typo it is.
//
// So a non-finite value keeps the documented default and says so in the log. A
// FINITE value outside the range still clamps, which is what a player nudging a
// number past its bound means.
float ReadClampedFloat(const cameraunlock::IniReader& ini, const char* section,
                       const char* key, float fallback, float lo, float hi) {
    const float value = ini.ReadFloat(section, key, fallback);
    if (!std::isfinite(value)) {
        Log::Line("config: %s.%s is not a finite number - using %.2f", section, key, fallback);
        return fallback;
    }
    if (value < lo || value > hi) {
        // Said out loud, because the log is what a player is asked to send when
        // a setting "does nothing". Silently substituting a number they did not
        // type is the fallback that wastes the next hour.
        const float clamped = value < lo ? lo : hi;
        Log::Line("config: %s.%s=%.2f is outside %.2f-%.2f - using %.2f",
                  section, key, value, lo, hi, clamped);
        return clamped;
    }
    return value;
}

// A hotkey is the one setting whose bad value is silent in game: the key simply
// never fires and there is nothing on screen to say why. Out of range falls back
// to the documented default and says so.
int ReadVirtualKey(const cameraunlock::IniReader& ini, const char* key, int fallback) {
    const int vk = ini.ReadHex("Hotkeys", key, fallback);
    if (vk >= kMinVirtualKey && vk <= kMaxVirtualKey) return vk;
    Log::Line("config: Hotkeys.%s=0x%X is not a virtual-key code - using 0x%02X",
              key, vk, fallback);
    return fallback;
}

}  // namespace

void Load(const std::string& exe_dir, Config& out) {
    g_iniPath = IniPath(exe_dir);

    cameraunlock::IniReader ini;
    if (!ini.Open(g_iniPath)) {
        Log::Line("config: no %s next to the game exe - using defaults", kIniName);
        return;
    }

    out.udp_port = ini.ReadInt("Network", "Port", out.udp_port);
    if (out.udp_port < kMinPort || out.udp_port > kMaxPort) {
        Log::Line("config: Network.Port=%d is outside %d-%d - using %d",
                  out.udp_port, kMinPort, kMaxPort, Config{}.udp_port);
        out.udp_port = Config{}.udp_port;
    }

    out.enable_on_startup = ini.ReadBool("General", "EnableOnStartup", out.enable_on_startup);
    out.world_space_yaw = ini.ReadBool("General", "WorldSpaceYaw", out.world_space_yaw);

    out.yaw_sensitivity = ReadClampedFloat(ini, "Sensitivity", "Yaw", out.yaw_sensitivity,
        kMinSensitivity, kMaxSensitivity);
    out.pitch_sensitivity = ReadClampedFloat(ini, "Sensitivity", "Pitch", out.pitch_sensitivity,
        kMinSensitivity, kMaxSensitivity);
    out.roll_sensitivity = ReadClampedFloat(ini, "Sensitivity", "Roll", out.roll_sensitivity,
        kMinSensitivity, kMaxSensitivity);

    out.invert_yaw = ini.ReadBool("Inversion", "Yaw", out.invert_yaw);
    out.invert_pitch = ini.ReadBool("Inversion", "Pitch", out.invert_pitch);
    out.invert_roll = ini.ReadBool("Inversion", "Roll", out.invert_roll);

    out.local_smoothing = ReadClampedFloat(ini, "Smoothing", "Local", out.local_smoothing,
        kMinSmoothing, kMaxSmoothing);
    out.remote_smoothing = ReadClampedFloat(ini, "Smoothing", "Remote", out.remote_smoothing,
        kMinSmoothing, kMaxSmoothing);

    out.position_enabled = ini.ReadBool("Position", "Enabled", out.position_enabled);
    out.position_sensitivity_x = ReadClampedFloat(ini, "Position", "SensitivityX",
        out.position_sensitivity_x, kMinPositionSensitivity, kMaxPositionSensitivity);
    out.position_sensitivity_y = ReadClampedFloat(ini, "Position", "SensitivityY",
        out.position_sensitivity_y, kMinPositionSensitivity, kMaxPositionSensitivity);
    out.position_sensitivity_z = ReadClampedFloat(ini, "Position", "SensitivityZ",
        out.position_sensitivity_z, kMinPositionSensitivity, kMaxPositionSensitivity);
    out.limit_x = ReadClampedFloat(ini, "Position", "LimitX", out.limit_x,
        kMinPositionLimitM, kMaxPositionLimitM);
    out.limit_y = ReadClampedFloat(ini, "Position", "LimitY", out.limit_y,
        kMinPositionLimitM, kMaxPositionLimitM);
    // Defaults to whatever LimitY was just read, not to its own default: an INI
    // written before LimitYDown existed carries only LimitY, and a player who
    // widens that expects the travel to widen both ways rather than getting an
    // asymmetric clamp with nothing in the log saying why.
    out.limit_y_down = ReadClampedFloat(ini, "Position", "LimitYDown", out.limit_y,
        kMinPositionLimitM, kMaxPositionLimitM);
    out.limit_z = ReadClampedFloat(ini, "Position", "LimitZ", out.limit_z,
        kMinPositionLimitM, kMaxPositionLimitM);
    out.limit_z_back = ReadClampedFloat(ini, "Position", "LimitZBack", out.limit_z_back,
        kMinPositionLimitM, kMaxPositionLimitM);

    // Two slots here, so `marker` must not resolve to anything: a file written
    // by a three-slot sibling mod, or by a release of this one from before the
    // game's own ADS reticle was confirmed (ads.h), otherwise selects a mode
    // that does not exist. ParseAdsMode lands it on the default instead.
    out.ads_mode = ParseAdsMode(
        ini.ReadString("View", "AdsMode", AdsModeValue(kDefaultAdsMode)).c_str(),
        /*allowMarker=*/false);

    out.yaw_mode_key = ReadVirtualKey(ini, "YawMode", out.yaw_mode_key);
    out.ads_mode_key = ReadVirtualKey(ini, "AdsMode", out.ads_mode_key);

    out.aim_probe = ini.ReadBool("Dev", "AimProbe", out.aim_probe);

    Log::Line("config: %s loaded (port=%d adsMode=%s)", kIniName, out.udp_port,
              AdsModeValue(out.ads_mode));
}

void WriteDefaultIfMissing(const std::string& exe_dir) {
    const std::string path = IniPath(exe_dir);

    // "x" is the C11 exclusive-create mode: the open FAILS if the file already
    // exists, rather than truncating it. An attributes test followed by a "wb"
    // open is the same intent with a window in it, and what falls into that
    // window is the player's hand-edited settings - two copies of the game
    // starting together, or a launcher writing the file while the first one
    // boots, and every key is back at its default with nothing said.
    const Config d{};
    FILE* f = std::fopen(path.c_str(), "wbx");
    if (!f) {
        // EEXIST is the ordinary case on every launch after the first: the
        // player already has an INI and it is left exactly as they wrote it.
        if (errno == EEXIST) return;
        Log::Line("config: could not write %s (errno %d) - defaults apply", path.c_str(), errno);
        return;
    }

    std::fprintf(f,
        "; High On Life Head Tracking\r\n"
        ";\r\n"
        "; Centring is done in your tracker (OpenTrack's Center bind, SteamVR, or\r\n"
        "; your phone app's CENTER button). The mod keeps no centre of its own.\r\n"
        "\r\n"
        "[Network]\r\n"
        "Port=%d\r\n"
        "\r\n"
        "[General]\r\n"
        "EnableOnStartup=%s\r\n"
        "; Yaw about the world up-axis (true) keeps the horizon level on a pitched\r\n"
        "; turn; camera-local yaw (false) leans it. Toggled in game with Page Down\r\n"
        "; or Ctrl+Shift+H; the toggle is not written back here.\r\n"
        "WorldSpaceYaw=%s\r\n"
        "\r\n"
        "[Sensitivity]\r\n"
        "Yaw=%.2f\r\n"
        "Pitch=%.2f\r\n"
        "Roll=%.2f\r\n"
        "\r\n"
        "[Inversion]\r\n"
        "Yaw=%s\r\n"
        "Pitch=%s\r\n"
        "Roll=%s\r\n"
        "\r\n"
        "[Smoothing]\r\n"
        "; Local applies to a tracker sending from this machine over loopback;\r\n"
        "; Remote applies to anything else, including a phone on WiFi and this\r\n"
        "; machine's own LAN address. Both cover rotation and position.\r\n"
        "Local=%.2f\r\n"
        "Remote=%.2f\r\n"
        "\r\n"
        "[Position]\r\n"
        "Enabled=%s\r\n"
        "SensitivityX=%.2f\r\n"
        "SensitivityY=%.2f\r\n"
        "SensitivityZ=%.2f\r\n"
        "; Metres. Z is asymmetric: more room to lean in than to pull back.\r\n"
        "LimitX=%.2f\r\n"
        "LimitY=%.2f\r\n"
        "LimitYDown=%.2f\r\n"
        "LimitZ=%.2f\r\n"
        "LimitZBack=%.2f\r\n"
        "\r\n"
        "[View]\r\n"
        "; What head tracking does while the sights are up. Cycled in game with\r\n"
        "; Insert or Ctrl+Shift+U, and saved back here when you do.\r\n"
        ";   paused   - tracking stands down for the aim (default, stock ADS)\r\n"
        ";   tracked  - tracking carries on; the game's own crosshair stays on\r\n"
        ";              the point your shot will hit\r\n"
        "AdsMode=%s\r\n"
        "\r\n"
        "[Hotkeys]\r\n"
        "; Virtual-key codes. The Ctrl+Shift chords do the same jobs and are not\r\n"
        "; configurable.\r\n"
        "YawMode=0x%02X\r\n"
        "AdsMode=0x%02X\r\n"
        "\r\n"
        "[Dev]\r\n"
        "; Logs how far away the world point is that the crosshair is drawn from.\r\n"
        "; A number that tracks whatever the weapon is pointed at is the aim\r\n"
        "; trace working; a constant is it not. Off otherwise.\r\n"
        "AimProbe=%s\r\n",
        d.udp_port,
        d.enable_on_startup ? "true" : "false",
        d.world_space_yaw ? "true" : "false",
        d.yaw_sensitivity, d.pitch_sensitivity, d.roll_sensitivity,
        d.invert_yaw ? "true" : "false",
        d.invert_pitch ? "true" : "false",
        d.invert_roll ? "true" : "false",
        d.local_smoothing, d.remote_smoothing,
        d.position_enabled ? "true" : "false",
        d.position_sensitivity_x, d.position_sensitivity_y,
        d.position_sensitivity_z,
        d.limit_x, d.limit_y, d.limit_y_down,
        d.limit_z, d.limit_z_back,
        AdsModeValue(kDefaultAdsMode),
        d.yaw_mode_key, d.ads_mode_key,
        d.aim_probe ? "true" : "false");
    std::fclose(f);
    Log::Line("config: wrote default %s", path.c_str());
}

void SaveAdsMode(AdsMode mode) {
    if (g_iniPath.empty()) return;
    if (!WritePrivateProfileStringA("View", "AdsMode", AdsModeValue(mode), g_iniPath.c_str())) {
        Log::Line("config: could not save AdsMode to %s (error %lu) - the setting applies for "
                  "this session but will not survive a restart",
                  g_iniPath.c_str(), GetLastError());
    }
}

}  // namespace hol_ht::config
