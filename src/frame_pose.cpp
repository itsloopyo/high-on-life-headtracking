// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "frame_pose.h"

#include <windows.h>

#include "ads_state.h"
#include "logging.h"
#include "mod_hotkeys.h"
#include "pose_shaping.h"

#include "cameraunlock/time/frame_clock.h"

namespace hol_ht::frame_pose {

namespace {

cameraunlock::time::FrameClock g_frameClock;

FramePose g_framePose;

// The lean fade and the zoom reference live on the game thread with the frame
// pose above.
AdsFade g_leanFade;
ZoomReference g_zoom;

float g_lastFovDeg = 0.0f;

void LogZoomTerms(const char* when, float fovDeg, float zoom) {
    Log::Line("zoom (%s): fov=%.2f base=%.2f (horizontal degrees, both FMinimalViewInfo::FOV) "
              "tan(fov/2)=%.4f tan(base/2)=%.4f factor=%.4f",
              when, fovDeg, g_zoom.BaseDeg(), ZoomReference::TanHalf(fovDeg),
              ZoomReference::TanHalf(g_zoom.BaseDeg()), zoom);
}

// Every term once at the hip, on the first gameplay frame whether or not a
// tracker is sending, where the factor has to read 1.0000. And once with the
// sights up and the FOV no longer moving, which is the number that says what
// the game's aim zoom is.
void LogZoomOnce(bool gameplay, bool aiming, float leanScale, float fovDeg, float zoom) {
    static bool s_hipLogged = false;
    static bool s_aimLogged = false;
    static bool s_unreadableLogged = false;
    if (!gameplay) return;
    if (!(fovDeg > 0.0f)) {
        if (!s_unreadableLogged) {
            s_unreadableLogged = true;
            Log::Line("zoom: the view's FOV could not be read - no zoom compensation");
        }
        return;
    }
    if (!aiming && !s_hipLogged) {
        s_hipLogged = true;
        LogZoomTerms("hip", fovDeg, zoom);
    }
    if (aiming && leanScale == 0.0f && fovDeg == g_lastFovDeg && !s_aimLogged) {
        s_aimLogged = true;
        LogZoomTerms("sights up", fovDeg, zoom);
    }
}

}  // namespace

const FramePose& Advance(std::uint64_t frame, std::uintptr_t controller,
                         game_state::Phase phase, Session& session,
                         bool trackingEnabled, float renderFovDeg) {
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
    // an exit that never arrives heals on the next frame.
    const bool aiming = ads_state::IsAimingDownSights(controller);
    const TrackingState state = DecideTracking(
        phase, trackingEnabled, live || havePosition, aiming);
    g_framePose.Verdict = state.verdict;
    g_framePose.Aiming = state.aiming;

    const bool applies = PoseApplies(state.verdict);
    if (!applies) g_leanFade.Reset();
    const float leanScale = applies ? g_leanFade.Update(state.aiming, GetTickCount64()) : 1.0f;

    // The zoom runs on every gameplay frame, tracker or not, so its terms are in
    // the log before anyone has to connect one.
    const bool gameplay = game_state::IsGameplay(phase);
    if (!gameplay) g_zoom.Reset();
    const float zoom = gameplay ? g_zoom.Update(renderFovDeg, !aiming && leanScale == 1.0f) : 1.0f;
    LogZoomOnce(gameplay, aiming, leanScale, renderFovDeg, zoom);
    g_lastFovDeg = renderFovDeg;
    g_framePose.Zoom = zoom;

    if (!applies) return g_framePose;

    ShapedPose raw;
    raw.yaw = yaw; raw.pitch = pitch; raw.roll = roll;
    raw.x = offX; raw.y = offY; raw.z = offZ;
    const ShapedPose shaped = ShapePose(raw, leanScale, zoom);

    g_framePose.HasRotation = live;
    g_framePose.HasPosition = havePosition;
    g_framePose.Yaw = shaped.yaw;
    g_framePose.Pitch = shaped.pitch;
    g_framePose.Roll = shaped.roll;
    g_framePose.OffX = shaped.x;
    g_framePose.OffY = shaped.y;
    g_framePose.OffZ = shaped.z;
    return g_framePose;
}

const FramePose& Latest() { return g_framePose; }

}  // namespace hol_ht::frame_pose
