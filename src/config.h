// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <string>

#include "ads.h"

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace hol_ht {

struct Config {
    int udp_port = 4242;
    bool enable_on_startup = true;

    // true = yaw turns about the world up-axis, so looking at the floor and
    // turning your head still pans across it. false = yaw turns about the
    // camera's own up-axis, which leans the horizon on a pitched turn.
    // Runtime-toggleable; this is only the value the mod starts in.
    bool world_space_yaw = true;

    float yaw_sensitivity = 1.0f;
    float pitch_sensitivity = 1.0f;
    float roll_sensitivity = 1.0f;
    bool invert_yaw = false;
    bool invert_pitch = false;
    bool invert_roll = false;

    // Smoothing is picked per connection from the packet source address: a
    // tracker on this machine (loopback) uses local_smoothing, a remote network
    // device uses remote_smoothing. Both cover rotation and position.
    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    bool position_enabled = true;
    float position_sensitivity_x = 1.0f;
    float position_sensitivity_y = 1.0f;
    float position_sensitivity_z = 1.0f;
    float limit_x = cameraunlock::PositionSettings{}.limit_x;
    float limit_y = cameraunlock::PositionSettings{}.limit_y;
    float limit_y_down = cameraunlock::PositionSettings{}.limit_y_down;
    float limit_z = cameraunlock::PositionSettings{}.limit_z;
    float limit_z_back = cameraunlock::PositionSettings{}.limit_z_back;

    // Dev only: log how far away the world point is that the crosshair is drawn
    // from. See aim_point.h - it is the one measurement that says whether the
    // game's aim trace is still reaching the crosshair on a given build, and is
    // off the rest of the time.
    bool aim_probe = false;

    // What head tracking does while the sights are up. Validated against the two
    // values this mod offers (ads.h); anything else - a typo, `marker` from a
    // three-slot sibling, or a mode renamed since an older release wrote the
    // file - lands on the default rather than on whichever branch happens to be
    // last, so a player never ends up with head tracking through their sights
    // that they did not ask for.
    AdsMode ads_mode = kDefaultAdsMode;

    // Virtual-key code for the yaw-mode toggle. Ctrl+Shift+H does the same job
    // and is not configurable.
    int yaw_mode_key = 0x22;  // VK_NEXT (Page Down)

    // Virtual-key code for the ADS-mode cycle. Ctrl+Shift+U does the same job
    // and is not configurable.
    int ads_mode_key = 0x2D;  // VK_INSERT
};

}  // namespace hol_ht

// Reading and writing HeadTracking.ini, which sits next to the game exe.
namespace hol_ht::config {

// Fill `out` from the INI, leaving each field at the default it arrived with
// when the file has no key for it. The INI is a system boundary, so every value
// is range-checked here and trusted everywhere above.
void Load(const std::string& exe_dir, Config& out);

// Write a fully commented INI of the defaults, unless the player already has
// one. Never overwrites, so a hand-edited file survives every launch.
void WriteDefaultIfMissing(const std::string& exe_dir);

// Write the ADS mode back to the INI, leaving every other key and every comment
// in the file alone. The cycle key is the setting's other half, so a mode picked
// mid-firefight has to survive the next launch.
void SaveAdsMode(AdsMode mode);

}  // namespace hol_ht::config
