// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "ads.h"
#include "game_state.h"

// Whether the head pose reaches the view this frame, and why not when it does
// not.
//
// Pulled out of the render hook as a pure function so the walk can be exercised
// without the game. Every one of its answers is a frame the player either sees
// their head in or does not.
namespace hol_ht {

enum class TrackingVerdict {
    // The head pose is applied in full.
    Active,
    // The sights are up in `paused` mode. The pose is still fed to the camera,
    // because it is being EASED off rather than switched off - see AdsFade - and
    // once it has gone the frame is the frame the game would have drawn on its
    // own, bar the head tilt: roll is left out of the fade in every mode, since
    // it moves neither the eye off the barrel nor the aim off the middle of the
    // frame (cameraunlock/ads/ads_blend.h).
    AdsSuspended,
    // The master toggle is off.
    Disabled,
    // A menu, a cinematic, or a player controller this profile has no flags for.
    NotGameplay,
    // The tracker has published nothing this frame.
    NoTracker,
};

struct TrackingState {
    TrackingVerdict verdict = TrackingVerdict::NotGameplay;
    // The sights are up. Reported in EVERY mode, including `paused` where the
    // gate is closed: the gate says whether tracking applies, this says what the
    // weapon is doing, and the per-frame code needs both - the aim marker is
    // placed from it.
    bool aiming = false;
};

// ADS is tested LAST, so a menu, a cinematic or a dead tracker still reports its
// own reason when both are true at once - and every earlier return leaves
// `aiming` false, because a stale flag through a menu would keep the marker
// drawing against a weapon that is not raised.
inline TrackingState DecideTracking(game_state::Phase phase, bool trackingEnabled,
                                    bool havePose, bool aiming, AdsMode mode) {
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
    s.verdict = (aiming && AdsSuspendsTracking(mode)) ? TrackingVerdict::AdsSuspended
                                                      : TrackingVerdict::Active;
    return s;
}

// A pose is fed to the camera in both of the first two verdicts. AdsSuspended
// needs it because suspending is an ease-out, not a switch: dropping the pose on
// the falling edge into ADS would throw away the smoothing state, and lowering
// the weapon would then swing the view back through the whole head angle.
inline bool PoseApplies(TrackingVerdict verdict) {
    return verdict == TrackingVerdict::Active || verdict == TrackingVerdict::AdsSuspended;
}

}  // namespace hol_ht
