// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "config.h"
#include "session.h"

// The mod's key bindings: the AGENTS.md nav-cluster defaults and their
// Ctrl+Shift chord alternatives, registered together so a keyboard without a
// nav cluster still reaches every action. Each binding does its work through
// view_hook or the session and says what it did in the log, so this is the only
// place that knows which key means what.
namespace hol_ht::hotkeys {

// Register the bindings and start polling.
void Register(const Config& config);

// Run any binding that has to touch state the game thread owns, and return
// whether one ran. Call from the game thread once per frame.
//
// The poller runs on its own thread, which is fine for the flags in view_hook -
// they are atomics read once per frame. It is NOT fine for the tracking mode:
// HeadTrackingSession::SetMode stores its atomic and then resets the position
// processor and the interpolator, which are plain floats the game thread is
// reading and writing inside Update(). So the key press is recorded here and
// the mode actually changes between frames.
bool ApplyPending(Session& session);

}  // namespace hol_ht::hotkeys
