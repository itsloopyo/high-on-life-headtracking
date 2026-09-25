// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Built with hol_ht and cameraunlock renamed on the command line (CMakeLists.txt),
// so the published reader and the core code it calls are a separate copy from
// today's, and nothing here can resolve to a symbol of the mod under test.

#include "oracle_api.h"

#include "config.h"

namespace hol_oracle {

PublishedConfig Load(const std::string& exe_dir) {
    hol_ht::Config c;
    hol_ht::config::Load(exe_dir, c);
    return PublishedConfig{
        c.udp_port,
        c.enable_on_startup,
        c.world_space_yaw,
        c.yaw_sensitivity,
        c.pitch_sensitivity,
        c.roll_sensitivity,
        c.invert_yaw,
        c.invert_pitch,
        c.invert_roll,
        c.local_smoothing,
        c.remote_smoothing,
        c.position_enabled,
        c.position_sensitivity_x,
        c.position_sensitivity_y,
        c.position_sensitivity_z,
        c.limit_x,
        c.limit_y,
        c.limit_y_down,
        c.limit_z,
        c.limit_z_back,
        c.aim_probe,
        static_cast<int>(c.ads_mode),
        c.yaw_mode_key,
        c.ads_mode_key,
    };
}

void WriteDefaultIfMissing(const std::string& exe_dir) { hol_ht::config::WriteDefaultIfMissing(exe_dir); }

}  // namespace hol_oracle
