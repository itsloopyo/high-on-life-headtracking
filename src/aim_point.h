// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

// Give the game's own crosshair the point the shot actually stops at.
//
// This mod does not project the reticle. AORPlayerCharacter::UpdateCrosshairLocation
// asks the equipped weapon's firing result for a world aim point and puts it
// through APlayerController::ProjectWorldLocationToScreen every frame, and that
// projection is on the presentation allow-list, so the crosshair is placed by
// the engine's own matrices from the eye the frame is drawn from. That is right
// if, and only if, the point being projected is where the round stops.
//
// As the game ships, it is not. The call at that one site passes
// bUseAimCorrections = false - `xor r8d, r8d` immediately before it - and with
// the flag clear every firing result returns aimStart + direction * range, a
// fixed ray end. Measured in game: 40.96 m to two decimals whether the weapon is
// pointed down a street or at the floor two metres away.
//
// With the eye sitting on the shot's own ray that costs nothing, because every
// point on the ray projects to the same pixel. Head lean is what separates them,
// and a fixed depth d0 then leaves an error of lean * (1/d0 - 1/d): zero at
// 40.96 m and growing at every other range. That is exactly the reticle that
// agrees at one distance and drifts either side of it.
//
// The fix is the game's own code rather than a trace of ours. The SAME function,
// with the flag set, runs UWorld::LineTraceSingleByChannel using the weapon's
// own collision channel with the shooter excluded, and returns the impact point;
// and the flag IS set on the path that spawns the player's projectile
// (UORFiringResult_Projectile::GetPlayerSpawnTransform, `mov r8b, 1`). So the
// hook sets it at the crosshair's call site and nowhere else. The crosshair is
// then drawn from the point the shot is aimed at, the game's own aim corrections
// included, and there is no second aim derivation in this mod to disagree with
// the first.
namespace hol_ht::aim_point {

// Hook USQFiringResultComponent::GetAimLocation. Returns false, having logged
// why, when the profile has no RVA for it or MinHook refuses - the crosshair
// then keeps the fixed ray end the game ships with, which is correct until the
// player leans.
//
// `logDistance` adds the dev measurement behind [Dev] AimProbe: how far away the
// aim point is, on a bounded schedule. A distance that tracks what the weapon is
// pointed at is this hook working; a constant is the flag not reaching the
// trace, which is what a new game build would break first.
bool Install(bool logDistance);

}  // namespace hol_ht::aim_point
