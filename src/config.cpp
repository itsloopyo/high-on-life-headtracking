// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"

#include <cerrno>
#include <cstdio>
#include <string>

#include <windows.h>

#include "legacy_config/legacy_config.h"
#include "logging.h"

namespace hol_ht::config {

namespace {

constexpr const char* kIniName = "HeadTracking.ini";

std::string IniPath(const std::string& exe_dir) {
    return exe_dir + "\\" + kIniName;
}

}  // namespace

void Load(const std::string& exe_dir, Config& out) {
    legacy::Config read;
    legacy::Load(exe_dir, read);

    out.udp_port = read.udp_port;
    out.enable_on_startup = read.enable_on_startup;
    out.world_space_yaw = read.world_space_yaw;
    out.yaw_sensitivity = read.yaw_sensitivity;
    out.pitch_sensitivity = read.pitch_sensitivity;
    out.roll_sensitivity = read.roll_sensitivity;
    out.invert_yaw = read.invert_yaw;
    out.invert_pitch = read.invert_pitch;
    out.invert_roll = read.invert_roll;
    out.local_smoothing = read.local_smoothing;
    out.remote_smoothing = read.remote_smoothing;
    out.position_enabled = read.position_enabled;
    out.position_sensitivity_x = read.position_sensitivity_x;
    out.position_sensitivity_y = read.position_sensitivity_y;
    out.position_sensitivity_z = read.position_sensitivity_z;
    out.limit_x = read.limit_x;
    out.limit_y = read.limit_y;
    out.limit_y_down = read.limit_y_down;
    out.limit_z = read.limit_z;
    out.limit_z_back = read.limit_z_back;
    out.aim_probe = read.aim_probe;
    out.yaw_mode_key = read.yaw_mode_key;
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
        "[Hotkeys]\r\n"
        "; Virtual-key codes. The Ctrl+Shift chords do the same jobs and are not\r\n"
        "; configurable.\r\n"
        "YawMode=0x%02X\r\n"
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
        d.yaw_mode_key,
        d.aim_probe ? "true" : "false");
    std::fclose(f);
    Log::Line("config: wrote default %s", path.c_str());
}

}  // namespace hol_ht::config
