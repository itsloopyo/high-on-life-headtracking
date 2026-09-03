// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "frame_pose.h"

#include <windows.h>

#include "ads_state.h"
#include "logging.h"
#include "mod_hotkeys.h"

#include "cameraunlock/time/frame_clock.h"

namespace hol_ht::frame_pose {

namespace {

cameraunlock::time::FrameClock g_frameClock;

FramePose g_framePose;

// The ADS transition and the pose the sights came up on. Both live on the game
// thread with the frame pose above, and both are dropped on every suppressed
// frame so the next aim re-enters cleanly.
AdsFade g_adsFade;
AdsEntryPose g_adsEntry;

// One line the first time the sights come up in each mode, naming what that mode
// does. This is what a player's log shows when they report that aiming does
// something they did not expect.
//
// Not once per aim. This is a shooter: a line each way on every edge is a pair
// of lines every few seconds of combat, which buries the build check and the
// hook lines the log exists for. The live pair is already in the 30s heartbeat
// (`ads=aiming/mode`), so an ongoing state is readable there instead.
void LogFirstAdsEntry(bool aiming, AdsMode mode) {
    if (!aiming) return;
    static unsigned s_logged = 0;
    const unsigned bit = 1u << static_cast<unsigned>(mode);
    if (s_logged & bit) return;
    s_logged |= bit;
    // Two slots, so this asks core's named rule rather than enumerating the enum
    // - which carries a third value this mod does not offer (ads.h).
    if (AdsSuspendsTracking(mode)) {
        Log::Line("ads: sights up - head tracking paused, view settling onto the aim");
    } else {
        Log::Line("ads: sights up - view settling onto the aim, head tracking carries on "
                  "from there, with the game's own crosshair on the point the shot hits");
    }
}

}  // namespace

const FramePose& Advance(std::uint64_t frame, std::uintptr_t controller,
                         game_state::Phase phase, Session& session,
                         bool trackingEnabled, AdsMode adsMode) {
    if (frame == g_framePose.Frame) return g_framePose;

    g_framePose = FramePose{};
    g_framePose.Frame = frame;

    // Before the session is advanced, and on this thread: the tracking-mode
    // cycle resets the position processor and the interpolator, which Update()
    // is about to read.
    hotkeys::ApplyPending(session);

    const bool updated = session.Update(g_frameClock.Tick());

    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    float offX = 0.0f, offY = 0.0f, offZ = 0.0f;
    const bool live = updated && session.GetRotation(yaw, pitch, roll);
    const bool havePosition = updated && session.GetPositionOffset(offX, offY, offZ);

    // Polled from the game's own flag every frame, never latched on an edge, so
    // an exit that never arrives heals on the next frame instead of stranding
    // the player in ADS behaviour.
    const bool aiming = ads_state::IsAimingDownSights(controller);
    const TrackingState state = DecideTracking(
        phase, trackingEnabled, live || havePosition, aiming, adsMode);
    g_framePose.Verdict = state.verdict;
    g_framePose.Aiming = state.aiming;

    if (!PoseApplies(state.verdict)) {
        // Menu, cinematic, master toggle, dead tracker: drop the transition and
        // the pose the sights came up on, so the next aim re-enters from where
        // the head is then rather than against a pose from before the
        // suppression.
        g_adsFade.Reset();
        g_adsEntry.Reset();
        return g_framePose;
    }

    // Raising the sights hands the view back to the gun: the head pose eases out
    // over a fraction of a second and the frame settles onto the aim, which is
    // where the crosshair already was. All three modes make that same swing and
    // differ in where the fade lands - nothing in `paused`, the entry-relative
    // pose in the other two. Roll is in neither fade; see BlendAdsPose.
    //
    // Both are asked in every mode, so the entry pose is dropped when the weapon
    // comes down whichever mode was live while it was up, and a mode cycled
    // mid-aim takes effect on that aim.
    LogFirstAdsEntry(state.aiming, adsMode);
    const float scale = g_adsFade.Update(state.aiming, GetTickCount64());
    const AdsEntryPose::Pose absolute{ pitch, yaw, roll, offX, offY, offZ };
    const AdsEntryPose::Pose relative = g_adsEntry.Relative(state.aiming, live, absolute);
    const AdsEntryPose::Pose blended = BlendAdsPose(adsMode, scale, absolute, relative);

    g_framePose.HasRotation = live;
    g_framePose.HasPosition = havePosition;
    g_framePose.Yaw = blended.yaw;
    g_framePose.Pitch = blended.pitch;
    g_framePose.Roll = blended.roll;
    g_framePose.OffX = blended.x;
    g_framePose.OffY = blended.y;
    g_framePose.OffZ = blended.z;
    return g_framePose;
}

const FramePose& Latest() { return g_framePose; }

}  // namespace hol_ht::frame_pose
