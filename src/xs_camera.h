// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

// The retro game at the start, where head tracking is off, and what it is
// waiting on.
//
// High On Life opens on an in-fiction first-person game. That game is not drawn
// by the player's camera: the engine runs a second pawn, AORXSPlayerCharacter,
// whose point of view a USceneCaptureComponent2D captures into a render target,
// and the render target is what the screen in the room displays. Head tracking
// the player's own view there only slides that flat picture around the frame;
// head tracking the capture is what looks around the retro game, and it works -
// AORXSPlayerCharacter::UpdateSceneCapture builds an FMinimalViewInfo every
// frame and hands it to USceneCaptureComponent2D::SetCameraView, its only caller
// in this build, and composing the pose onto that view is the same operation
// view_hook.cpp performs on the player's view.
//
// What stops it shipping is the crosshair. The retro game's is a fixed mark at
// the middle of the picture, which is exactly right in the shipped game because
// the capture looks precisely where the retro gun aims, so the middle of the
// picture IS the shot. Move that camera and the mark stops being the shot. The
// engine will not fix it for us the way it does everywhere else in this game:
// an XsProbe run across the whole section on 2026-09-01 saw
// APlayerController::ProjectWorldLocationToScreen fire zero times from the
// crosshair's call site, so there is no game-side projection to hand the
// head-tracked view to.
//
// That leaves a mark this mod draws itself, and it cannot draw one here: the
// shared aim marker is Direct3D 11 only
// (cameraunlock/rendering/aim_marker_dx11.h) and High On Life runs on Direct3D
// 12, where it never attaches to the swap chain. A crosshair that is not where
// the rounds go is the one thing this mod must never ship, so until there is a
// Direct3D 12 marker the section is left exactly as the game draws it: the
// capture untouched, and the player's view untouched with it.
//
// This is a hole waiting on one piece of work in cameraunlock-core, not a
// judgement about the section. That same work fixes `marker` ADS mode, which
// has never drawn a mark in this game for the same reason. The camera injection
// is in git and every offset it needs is in .lab/NOTES.md.
namespace hol_ht::xs_camera {

// Hook USceneCaptureComponent2D::SetCameraView, which is how the section is
// recognised at all: AORXSPlayerCharacter::UpdateSceneCapture is its only caller
// in this build, and that runs only while an XS controller is driving an
// AORXSPlayerCharacter. Returns false, having logged why, if the profile has no
// RVA for it or MinHook refuses - and then the section goes unrecognised and the
// player's view head-tracks through it like anywhere else.
bool Install();

// True when the retro game's camera was asked for on `frame`, which is when the
// player's own view must be left alone.
bool OwnsPose(std::uint64_t frame);

}  // namespace hol_ht::xs_camera
