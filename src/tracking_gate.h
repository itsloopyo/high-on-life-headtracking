// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "game_state.h"

// Whether the head pose reaches the view this frame, and why not when it does
// not.
//
// Pulled out of the render hook as a pure function so the walk can be exercised
// without the game. Every one of its answers is a frame the player either sees
// their head in or does not.
//
// Aiming down sights is not one of the reasons. Head tracking carries straight
// on through the aim; the gate only reports the sights so the lean can be eased
// out while they are up (pose_shaping.h).
namespace hol_ht {

enum class TrackingVerdict {
    // The head pose is applied.
    Active,
    // The master toggle is off.
    Disabled,
    // A menu, a cinematic, or a player controller this profile has no flags for.
    NotGameplay,
    // The tracker has published nothing this frame.
    NoTracker,
};

struct TrackingState {
    TrackingVerdict verdict = TrackingVerdict::NotGameplay;
    // The sights are up, on a frame the pose applies to.
    bool aiming = false;
};

// ADS is read LAST, so every earlier return leaves `aiming` false: a stale flag
// through a menu would hold the lean eased out against a weapon that is not
// raised.
inline TrackingState DecideTracking(game_state::Phase phase, bool trackingEnabled,
                                    bool havePose, bool aiming) {
    TrackingState s;
    if (!trackingEnabled) {
        s.verdict = TrackingVerdict::Disabled;
        return s;
    }
    if (!game_state::IsGameplay(phase)) {
        s.verdict = TrackingVerdict::NotGameplay;
        return s;
    }
    if (!havePose) {
        s.verdict = TrackingVerdict::NoTracker;
        return s;
    }
    s.aiming = aiming;
    s.verdict = TrackingVerdict::Active;
    return s;
}

inline bool PoseApplies(TrackingVerdict verdict) {
    return verdict == TrackingVerdict::Active;
}

}  // namespace hol_ht
