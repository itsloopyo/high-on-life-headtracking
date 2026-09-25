// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "config.h"
#include "session.h"

#include "cameraunlock/tracking/tracking_mode.h"

// The mod's key bindings: the three key lists in HeadTracking.ini, each holding
// its nav-cluster key and its Ctrl+Shift chord by default. Each binding does its
// work through view_hook or the session and says what it did in the log, so this
// is the only place that knows which key means what.
namespace hol_ht::hotkeys {

// Register the bindings and start polling. `applied` is the tracking mode the
// session started in.
void Register(const Config& config, cameraunlock::TrackingMode applied);

// Apply a tracking mode the cycle key asked for, and return whether one was
// applied. Call from the game thread once per frame.
//
// The poller runs on its own thread, which is fine for the flags in view_hook -
// they are atomics read once per frame. It is NOT fine for the tracking mode:
// HeadTrackingSession::SetMode stores its atomic and then resets the position
// processor and the interpolator, which are plain floats the game thread is
// reading and writing inside Update(). So the key press stores the mode it wants
// and the mode actually changes between frames.
bool ApplyPending(Session& session);

}  // namespace hol_ht::hotkeys
