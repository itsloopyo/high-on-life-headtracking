// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <string>

#include "cameraunlock/config/config_concepts.g.h"
#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"
#include "cameraunlock/tracking/tracking_mode.h"

namespace hol_ht {

struct Config {
    int udp_port = 4242;
    bool enable_on_startup = true;

    // true = yaw turns about the world up-axis, so looking at the floor and
    // turning your head still pans across it. false = yaw turns about the
    // camera's own up-axis, which leans the horizon on a pitched turn.
    bool world_space_yaw = true;

    // Smoothing is picked per connection from the packet source address: a
    // tracker on this machine (loopback) uses local_smoothing, a remote network
    // device uses remote_smoothing. Both cover rotation and position.
    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    // The tracking mode at startup, as the pair the mode hotkey saves.
    bool rotation_enabled = true;
    bool position_enabled = true;

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

    std::string toggle_key =
        cameraunlock::config::schema::ConceptTraits<cameraunlock::config::schema::Concept::ToggleKey>::kCanonicalDefault;
    std::string cycle_tracking_mode_key = cameraunlock::config::schema::ConceptTraits<
        cameraunlock::config::schema::Concept::CycleTrackingModeKey>::kCanonicalDefault;
    std::string yaw_mode_key =
        cameraunlock::config::schema::ConceptTraits<cameraunlock::config::schema::Concept::YawModeKey>::kCanonicalDefault;
};

}  // namespace hol_ht

// HeadTracking.ini, next to the game exe, in cameraunlock-core's canonical
// config format. One ConfigOwner reads and writes it; nothing else in the mod
// touches the file.
namespace hol_ht::config {

// The rows of HeadTracking.ini.
cameraunlock::config::ConfigTable<Config> Table();

// What the renderer writes above the rows.
cameraunlock::config::RenderHeader Header();

// The owner's options for the file at `path`: the table, the frozen legacy
// import and the header.
cameraunlock::config::ConfigOwnerOptions<Config> OwnerOptions(const std::wstring& path);

// Reads, converts or creates HeadTracking.ini in `exe_dir`, logs what the owner
// reports, and returns the settings the session runs on. Call once, from the
// bootstrap thread, with the log open.
Config Load(const std::wstring& exe_dir);

// Save the value a hotkey has just applied. The session keeps it whether or not
// the save succeeds; a failed save is logged. Called from the hotkey thread.
void SaveWorldSpaceYaw(bool world_space_yaw);
void SaveTrackingMode(cameraunlock::TrackingMode mode);

}  // namespace hol_ht::config
