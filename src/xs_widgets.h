// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

// The retro game's own crosshair, and moving it.
//
// XenoSlaughter - the in-fiction first-person game the player is playing at the
// start - draws its crosshair as a UMG Image called `Crosshair` inside
// `XS_AmmoWidget_C`, its HUD widget blueprint. That was found by walking
// GUObjectArray in the running game, because the HUD is Blueprint and nothing
// in the PDB names it.
//
// It is a FIXED mark at the middle of the picture, and the engine never projects
// it - measured, not assumed: ProjectWorldLocationToScreen fires zero times from
// the crosshair's call site across the whole section. That is correct in the
// shipped game, where the retro camera looks exactly where the retro gun aims,
// so the middle of the picture IS the shot. Head tracking moves that camera off
// the aim, and the mark has to follow.
//
// So the mark is MOVED rather than drawn over: one crosshair on screen instead
// of two, and no swap chain to attach to - which matters, because the shared aim
// marker is Direct3D 11 only and this game runs Direct3D 12. The mechanism is
// core's cameraunlock/unreal/umg_reticle.h; this file is the wiring - which
// widget, and this mod's log channel.
namespace hol_ht::xs_widgets {

// Bind UWidget::SetRenderTranslation from the build profile. Returns false,
// having logged why, when the profile has no RVA for it - the crosshair is then
// never moved, which is what keeps the camera injection switched off too.
bool Install();

// Find the live crosshair widget, if it is not already held. Costs one pass over
// the object table, so it is called only while the section is on screen, and
// only until it succeeds. Returns false when the widget is not there, which is
// the honest answer on a build that renamed it.
bool EnsureWidget();

// Offset the crosshair from where the game laid it out, x right and y down, in
// the crosshair's own local units - the same units PictureHalfExtent returns.
// Pass (0, 0) to put it back; never leave it parked at the last offset when
// tracking stops.
bool Move(float screenDx, float screenDy);

// Half the retro picture's width and height, in the units Move() takes -
// measured from the HUD's own Slate geometry rather than from the backbuffer, so
// nothing here has to know the window size or the DPI scale. False until Slate
// has laid the HUD out.
bool PictureHalfExtent(float& halfWidth, float& halfHeight);

// True once the widget is held and the setter is bound, which is the condition
// for head tracking to run in this section at all.
bool Ready();

// Drop the widget. The next EnsureWidget() looks again - called when the section
// ends, because the widget belongs to a level that will be torn down.
void Forget();

}  // namespace hol_ht::xs_widgets
