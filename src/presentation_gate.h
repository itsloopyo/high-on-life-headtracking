// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstddef>
#include <cstdint>

// Is this view query drawing something, or deciding something?
//
// The look/aim decoupling has two layers in this game. The first is the
// GetPlayerViewPoint caller gate in view_hook.cpp: only the call inside
// ULocalPlayer::GetViewPoint is a candidate at all, so interaction traces, the
// audio listener and AI perception never see the head pose.
//
// This is the second layer, and it exists because GetViewPoint is not purely a
// render-path function. It is reached from ULocalPlayer::GetProjectionData,
// which serves the frame being drawn AND every world-to-screen projection the
// game asks for - including four that are gameplay decisions:
//
//   AOREncounterManager::GetConditionalSpawnPoint       where enemies may appear
//   AORInteractionStationManager::EnableFocus           what an interaction focuses
//   ASQInventoryItem_Chainsaw::OnLOSTraceAllComplete    what the chainsaw grapples
//   UORPlayerGazeComponent::PaddedLineOfSightCheck      what the player "can see"
//
// Injecting the head pose into those would make where the player looks change
// what the game does. So the gate walks a few frames of the stack and injects
// only when a listed drawing consumer is on it - the build profile's
// kPresentationCallerRvas. Anything unlisted projects through the clean view.
//
// The allow-list direction is deliberate. A consumer missed off it draws
// through the clean view, which is a HUD element that drifts under head
// movement - visible, cosmetic, and reported. A gameplay caller missed off a
// deny-list would silently couple the player's head to the simulation.
namespace hol_ht::presentation {

// Set up the allow-list and the stack depth. view_hook passes the active build
// profile's values; the tests pass their own, which is the only reason this
// takes arguments rather than reading Offsets() itself.
void Init(const std::uintptr_t* rvas, std::size_t rvaCount, std::uint32_t stackFrames);

// The matching half of the gate, split out so it can be exercised without a
// game: given captured stack frames, the listed consumer among them, or 0.
// Frames outside the game module are ignored (ModuleRvaInRange).
std::uintptr_t MatchDrawingCaller(void* const* frames, unsigned long count);

// Is a listed drawing consumer on the current call stack? Call only from inside
// the GetPlayerViewPoint hook, and only once the cheap return-address test has
// already passed - this walks the stack.
//
// The walk is why this mod must not hook anything between a listed consumer and
// GetPlayerViewPoint: a detour puts a frame with no unwind data of the game's on
// the stack, RtlCaptureStackBackTrace stops there, and the consumer above it
// silently stops matching. See view_hook.cpp.
bool CallerIsDrawing();

// One line naming the frames of a stack the gate rejected, for the first few
// distinct rejections. This is what surfaces a drawing consumer that was missed
// off the list: its HUD element drifts, and its RVA is in the log.
void LogUnmatched();

}  // namespace hol_ht::presentation
