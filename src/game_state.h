// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

// Is the player playing right now?
//
// Every read here is off the APlayerController pointer the GetPlayerViewPoint
// hook already holds, so the whole gate is three guarded loads and no sampling
// thread. Three things have to be true for tracking to run:
//
//   - the controller is one of the game's gameplay player controllers, not the
//     main menu's, checked by vtable pointer so the two game-specific flags
//     below are never read off a class that does not have them
//   - AORPlayerController::bPauseMenuActive is clear
//   - APlayerController::bShowMouseCursor is clear, which UE raises whenever
//     input belongs to a menu rather than the player
//
// Loading screens need no gate of their own: there is no player controller to
// call GetPlayerViewPoint during one.
namespace hol_ht::game_state {

enum class Phase {
    Gameplay,     // tracking runs
    Menu,         // pause menu, or a raised mouse cursor
    Cinematic,    // a scripted cinematic, composed at a chosen framing
    UnknownClass, // a player controller this profile has no flags for
};

// Classify the controller the hook was called on.
Phase Classify(std::uintptr_t controller);

inline bool IsGameplay(Phase p) { return p == Phase::Gameplay; }

// One log line per transition, so a session log shows which scenes the gate
// caught without a heartbeat having to land inside one.
void LogTransition(Phase phase);

const char* PhaseName(Phase p);

}  // namespace hol_ht::game_state
