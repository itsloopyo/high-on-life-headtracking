// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/ads/ads_blend.h"
#include "cameraunlock/ads/ads_fade.h"
#include "cameraunlock/ads/ads_mode.h"
#include "cameraunlock/ads/entry_pose.h"

// The shared aim-down-sights module, under this mod's namespace.
//
// The cycle, its value strings, its toast wording, the transition shape and the
// entry-relative pose are a fleet-wide contract rather than a High On Life
// decision - a player who learns Insert in one shooter has to find the same
// slots in the same order here - so they live in cameraunlock-core and this file
// only says which of them this game gets.
//
// **High On Life ships TWO slots**, which is core's shape for a game whose own
// ADS reticle is an aim indicator at the impact point and is reachable by the
// mod while the sights are up. Both halves of that are true here, and both were
// settled in game rather than read out of the binary:
//
// - The game's crosshair marks the impact point. AORPlayerCharacter::
//   UpdateCrosshairLocation projects the equipped weapon's own world aim point
//   every frame and writes the result to CrosshairLocation; that projection is
//   in the presentation list (build_profile.h), so the crosshair is placed from
//   the eye the frame is drawn from, and aim_point.h gives it the traced impact
//   point rather than the fixed ray end the call site asks for.
// - It stays visible through an aim. That could not be answered from the exe -
//   the crosshair widgets are Blueprint-driven and their visibility runs through
//   UORWidget_HUDMaster::SetCrosshairVisibility, which native code never calls -
//   so it was answered by aiming down the sights and looking: the mark is still
//   on screen, and still on the world point the shot is going to.
//
// So `marker` is not offered anywhere: not in the cycle, not in the INI, not in
// the README. A second mark drawn on the same point the game is already marking
// is a mode whose only effect is a duplicate.
namespace hol_ht {

using cameraunlock::ads::AdsEntryPose;
using cameraunlock::ads::AdsFade;
using cameraunlock::ads::AdsMode;
using cameraunlock::ads::AdsModeToast;
using cameraunlock::ads::AdsModeValue;
using cameraunlock::ads::AdsSuspendsTracking;
using cameraunlock::ads::BlendAdsPose;
using cameraunlock::ads::kDefaultAdsMode;
using cameraunlock::ads::NextAdsModeTwoSlot;
using cameraunlock::ads::ParseAdsMode;

}  // namespace hol_ht
