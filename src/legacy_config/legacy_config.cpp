// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Frozen. See legacy_config.h.

#include "legacy_config.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <string>

#include <windows.h>

#include "logging.h"

#include "cameraunlock/config/ini_reader.h"

namespace hol_ht::legacy {

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
    cameraunlock::IniReader ini;
    if (!ini.Open(IniPath(exe_dir))) {
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

    out.yaw_mode_key = ReadVirtualKey(ini, "YawMode", out.yaw_mode_key);

    out.aim_probe = ini.ReadBool("Dev", "AimProbe", out.aim_probe);

    Log::Line("config: %s loaded (port=%d)", kIniName, out.udp_port);
}

}  // namespace hol_ht::legacy
