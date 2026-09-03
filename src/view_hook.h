// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

#include "ads.h"
#include "config.h"
#include "session.h"

// The APlayerController::GetPlayerViewPoint hook: the one place the head pose
// reaches the game. Everything it needs from the rest of the mod arrives once,
// at Install(), so the hot path owns no state that the bootstrap also writes.
namespace hol_ht::view_hook {

// All three must outlive the hook, which the bootstrap satisfies by owning them
// for the life of the process - the hook is never uninstalled while the game is
// running, so there is no window in which one could be freed under it.
struct Dependencies {
    const Config* config;
    Session*      session;
    cameraunlock::UdpReceiver* receiver;
};

// Create and enable the trampoline against the active build profile's RVA.
// Returns false, having logged why, if MinHook refuses; the caller then leaves
// the game vanilla.
bool Install(const Dependencies& deps);

bool TrackingEnabled();
void SetTrackingEnabled(bool enabled);

// true = world-space yaw (horizon-locked, FRotator addition); false = camera-
// local yaw (quaternion post-multiply, leans on pitched turns).
bool WorldSpaceYaw();
void SetWorldSpaceYaw(bool worldSpaceYaw);

// What head tracking does while the sights are up. Read once per frame by the
// hook, so a mode cycled mid-aim takes effect on that aim rather than the next.
AdsMode GetAdsMode();
void SetAdsMode(AdsMode mode);

// The engine's game-thread frame number, or the hook's own call count on a
// build profile with no GFrameCounter. Every consumer of a frame's view sees
// one pose - see frame_pose.h.
std::uint64_t CurrentFrameNumber();

// The clean (un-tracked) camera location of the most recent injected frame, in
// UE units. aim_point.h measures the distance to the crosshair's aim point from
// here. False before the first injected frame.
bool CleanCameraLocation(float& x, float& y, float& z);

}  // namespace hol_ht::view_hook
