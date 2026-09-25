// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Frozen. See legacy_import.h.

#include "legacy_import.h"

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

#include "legacy_config.h"

#include "cameraunlock/input/key_bindings.h"

namespace hol_ht::legacy {

namespace {

namespace cfg = ::cameraunlock::config;
using ::cameraunlock::input::FormatKeyBindings;
using ::cameraunlock::input::KeyModifiers;

constexpr KeyModifiers kChord = KeyModifiers::kCtrl | KeyModifiers::kShift;

// The keys the frozen reader's build bound in code rather than in the file.
constexpr int kVkEnd = 0x23;
constexpr int kVkPageUp = 0x21;
constexpr int kVkY = 0x59;
constexpr int kVkG = 0x47;
constexpr int kVkH = 0x48;

constexpr const char* kIniName = "\\HeadTracking.ini";

cfg::ImportResult Run(const cfg::LegacyInput& input, hol_ht::Config& out) {
    // The frozen reader takes the folder and names the file itself.
    const std::string& path = input.ansi_path;
    const std::size_t name_length = std::strlen(kIniName);
    if (path.size() < name_length || _stricmp(path.c_str() + path.size() - name_length, kIniName) != 0) {
        throw std::invalid_argument("the legacy import reads HeadTracking.ini only, not " + path);
    }
    const std::size_t name_at = path.size() - name_length;
    // IniReader::Open's own test: without it the frozen reader ran on its defaults.
    const bool present = GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;

    Config read;
    Load(path.substr(0, name_at), read);

    out.udp_port = read.udp_port;
    out.enable_on_startup = read.enable_on_startup;
    out.world_space_yaw = read.world_space_yaw;
    out.local_smoothing = read.local_smoothing;
    out.remote_smoothing = read.remote_smoothing;
    // [Position] Enabled picked the startup mode and nothing else: the mode cycle
    // still reached both position modes.
    out.rotation_enabled = true;
    out.position_enabled = read.position_enabled;
    out.limit_x = read.limit_x;
    out.limit_y = read.limit_y;
    out.limit_y_down = read.limit_y_down;
    out.limit_z = read.limit_z;
    out.limit_z_back = read.limit_z_back;
    out.aim_probe = read.aim_probe;

    // End, Page Up and the Ctrl+Shift chords were bound in code; only the yaw
    // key was in the file, and the reader keeps it inside 0x01-0xFE.
    out.toggle_key = FormatKeyBindings({{KeyModifiers::kNone, kVkEnd}, {kChord, kVkY}});
    out.cycle_tracking_mode_key = FormatKeyBindings({{KeyModifiers::kNone, kVkPageUp}, {kChord, kVkG}});
    out.yaw_mode_key = FormatKeyBindings({{KeyModifiers::kNone, read.yaw_mode_key}, {kChord, kVkH}});

    // Every one of these shipped as identity, so nothing is folded into the
    // mod's axis code and a value the player changed is dropped.
    const Config shipped{};
    std::vector<cfg::DroppedValue> dropped;
    std::vector<cfg::PoseShapingValue> pose;
    cfg::LegacyPoseShaping(read.yaw_sensitivity, shipped.yaw_sensitivity, "Sensitivity", "Yaw", pose, dropped);
    cfg::LegacyPoseShaping(read.pitch_sensitivity, shipped.pitch_sensitivity, "Sensitivity", "Pitch", pose, dropped);
    cfg::LegacyPoseShaping(read.roll_sensitivity, shipped.roll_sensitivity, "Sensitivity", "Roll", pose, dropped);
    cfg::LegacyPoseShaping(read.invert_yaw, shipped.invert_yaw, "Inversion", "Yaw", pose, dropped);
    cfg::LegacyPoseShaping(read.invert_pitch, shipped.invert_pitch, "Inversion", "Pitch", pose, dropped);
    cfg::LegacyPoseShaping(read.invert_roll, shipped.invert_roll, "Inversion", "Roll", pose, dropped);
    cfg::LegacyPoseShaping(read.position_sensitivity_x, shipped.position_sensitivity_x, "Position", "SensitivityX",
                           pose, dropped);
    cfg::LegacyPoseShaping(read.position_sensitivity_y, shipped.position_sensitivity_y, "Position", "SensitivityY",
                           pose, dropped);
    cfg::LegacyPoseShaping(read.position_sensitivity_z, shipped.position_sensitivity_z, "Position", "SensitivityZ",
                           pose, dropped);

    return present ? cfg::ImportResult::Imported(std::move(dropped), std::move(pose))
                   : cfg::ImportResult::Absent(std::move(dropped), std::move(pose));
}

}  // namespace

cfg::LegacyImport<hol_ht::Config> Import() {
    cfg::LegacyImport<hol_ht::Config> import;
    import.run = &Run;
    import.keys = {
        {"Network", "Port"},
        {"General", "EnableOnStartup"},
        {"General", "WorldSpaceYaw"},
        {"Sensitivity", "Yaw"},
        {"Sensitivity", "Pitch"},
        {"Sensitivity", "Roll"},
        {"Inversion", "Yaw"},
        {"Inversion", "Pitch"},
        {"Inversion", "Roll"},
        {"Smoothing", "Local"},
        {"Smoothing", "Remote"},
        {"Position", "Enabled"},
        {"Position", "SensitivityX"},
        {"Position", "SensitivityY"},
        {"Position", "SensitivityZ"},
        {"Position", "LimitX"},
        {"Position", "LimitY"},
        {"Position", "LimitYDown"},
        {"Position", "LimitZ"},
        {"Position", "LimitZBack"},
        {"Hotkeys", "YawMode"},
        {"Dev", "AimProbe"},
    };
    return import;
}

}  // namespace hol_ht::legacy
