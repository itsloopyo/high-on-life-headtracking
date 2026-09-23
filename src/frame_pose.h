// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

#include "game_state.h"
#include "session.h"
#include "tracking_gate.h"

// The head pose for one engine frame, decided once and reused.
//
// GetPlayerViewPoint runs several times in one frame: twice for the frame being
// drawn, once for the crosshair, once per world-anchored widget layer. They must
// all use the SAME head pose, or the crosshair is projected through a slightly
// different view than the one it is drawn over and slides against the world by a
// fraction of a degree every frame. So the session is advanced once per engine
// frame and the result is reused.
//
// The walk is here rather than in the hook because it is the whole of what a
// frame decides - tracker, gameplay gate, sights, zoom - and none of it touches
// the engine. The hook passes in what it knows and applies what comes back.
namespace hol_ht::frame_pose {

struct FramePose {
    std::uint64_t Frame = ~0ull;
    bool  HasRotation = false;
    float Yaw = 0.0f, Pitch = 0.0f, Roll = 0.0f;
    bool  HasPosition = false;
    float OffX = 0.0f, OffY = 0.0f, OffZ = 0.0f;
    TrackingVerdict Verdict = TrackingVerdict::NotGameplay;
    // The sights are up, on a frame the pose applies to.
    bool  Aiming = false;
    // The FOV zoom factor the pose was scaled by, 1 at the hip.
    float Zoom = 1.0f;
};

// The pose for `frame`, computing it on the first call of a frame and returning
// the cached one after that. `renderFovDeg` is FMinimalViewInfo::FOV for this
// view query, or 0 when it could not be read.
//
// Caller holds nothing: GetPlayerViewPoint is a game-thread function and every
// consumer that reaches it runs on the game thread, so the cache has one writer.
const FramePose& Advance(std::uint64_t frame, std::uintptr_t controller,
                         game_state::Phase phase, Session& session,
                         bool trackingEnabled, float renderFovDeg);

// What Advance() last produced, for the heartbeat - which fires on view queries
// that never reach the drawing path and so cannot be handed a fresh one.
const FramePose& Latest();

}  // namespace hol_ht::frame_pose
