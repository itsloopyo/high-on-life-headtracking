// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

// Are the sights up, as the game itself understands it?
//
// The game keeps the answer explicitly. AORCharacter::SetADSState writes one
// byte, AORCharacter::ADSOn, and the 1P weapon anim instance and the input
// component's ADS sensitivity both read it - so it is a statement about the
// sights rather than about magnification, and it is right for every weapon
// whether or not that weapon's aim zooms.
//
// Reaching it costs two guarded loads off the APlayerController the
// GetPlayerViewPoint hook is already holding: AController::Character, then the
// byte. No hook of its own, and no sampling thread.
//
// POLLED, never latched. Enter and exit events fire unevenly - a state machine
// that transitions without one, an aim released while firing - and a latched
// flag that misses an edge either strands the player in ADS behaviour or leaks
// hip-fire tracking into the aim. An unreadable frame answers "not aiming",
// because failing toward stock is the safe direction.
namespace hol_ht::ads_state {

// `controller` is the APlayerController the view query was made on.
bool IsAimingDownSights(std::uintptr_t controller);

}  // namespace hol_ht::ads_state
