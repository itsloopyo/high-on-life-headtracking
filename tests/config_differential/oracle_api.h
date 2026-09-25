// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <string>

// The oracle: the HeadTracking.ini reader of the newest published build, the
// rolling `dev` pre-release built from c24cc5a against cameraunlock-core
// 9d86b01, compiled only into this test. oracle/src/ holds that build's
// config.h, config.cpp, ads.h and logging.h, and oracle/core/ the core headers
// they include that core has changed since, all byte for byte as the tag has
// them (provenance in differential_tests.cpp). oracle_api.cpp compiles them with
// their namespaces renamed, so they link beside today's code.
namespace hol_oracle {

// hol_ht::Config as that build declared it, field for field.
struct PublishedConfig {
    int udp_port;
    bool enable_on_startup;
    bool world_space_yaw;
    float yaw_sensitivity;
    float pitch_sensitivity;
    float roll_sensitivity;
    bool invert_yaw;
    bool invert_pitch;
    bool invert_roll;
    float local_smoothing;
    float remote_smoothing;
    bool position_enabled;
    float position_sensitivity_x;
    float position_sensitivity_y;
    float position_sensitivity_z;
    float limit_x;
    float limit_y;
    float limit_y_down;
    float limit_z;
    float limit_z_back;
    bool aim_probe;
    // cameraunlock::ads::AdsMode at 9d86b01: 0 paused, 1 marker, 2 tracked.
    int ads_mode;
    int yaw_mode_key;
    int ads_mode_key;
};

// The published config::Load on a default Config, as its bootstrap called it.
PublishedConfig Load(const std::string& exe_dir);

// The published config::WriteDefaultIfMissing: that build's first-run file.
void WriteDefaultIfMissing(const std::string& exe_dir);

}  // namespace hol_oracle
