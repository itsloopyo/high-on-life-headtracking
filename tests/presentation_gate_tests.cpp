// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// The gate that decides whether a view query is drawing something or deciding
// something.
//
// This is the one thing standing between where the player looks and what the
// game does: a consumer wrongly admitted couples head movement to encounter
// spawning, interaction focus, the chainsaw's line of sight and the gaze
// component. The allow-list direction is the safety property, so what is locked
// here is that an unlisted caller is rejected, including one that only differs
// from a listed one by being outside the game module.

#include <cstdint>

#include "presentation_gate.h"
#include "test_harness.h"

#include "cameraunlock/unreal/ue_runtime.h"

namespace {

namespace ue = ::cameraunlock::unreal;
using hol_ht::presentation::Init;
using hol_ht::presentation::MatchDrawingCaller;

// A fake module the RVAs below are resolved against, the same trick
// ue_probe_tests uses. Nothing is dereferenced - the gate only does arithmetic
// and comparison on the frame addresses.
constexpr std::uintptr_t kBase = 0x10000000ull;
constexpr std::uintptr_t kSize = 0x01000000ull;

void* Addr(std::uintptr_t rva) { return reinterpret_cast<void*>(kBase + rva); }

// Two drawing consumers and a zero, which is how a profile pads unused slots.
constexpr std::uintptr_t kAllowed[] = { 0x1000, 0x2000, 0x0 };

void SetUp() {
    ue::SetRuntime(kBase, kBase + kSize, ue::UObjectGlobalsLayout{});
    Init(kAllowed, sizeof(kAllowed) / sizeof(kAllowed[0]), 8);
}

void ListedCallerIsMatched() {
    void* frames[] = { Addr(0x9999), Addr(0x1000) };
    CHECK_MSG(MatchDrawingCaller(frames, 2) == 0x1000, "a listed consumer on the stack matches");
}

// The whole point of the allow-list: the four gameplay callers are not on it,
// so they project through the clean view.
void UnlistedCallerIsRejected() {
    void* frames[] = { Addr(0x5000), Addr(0x6000) };
    CHECK_MSG(MatchDrawingCaller(frames, 2) == 0, "an unlisted consumer does not match");
}

// Which consumer matched decides whether the aim marker is asked for this
// frame, so the gate has to name it rather than just answer yes.
void TheMatchedRvaIsReported() {
    void* frames[] = { Addr(0x2000) };
    CHECK_MSG(MatchDrawingCaller(frames, 1) == 0x2000, "the matched consumer is the one reported");
}

// A padding slot must never match. Frames outside the module resolve to RVA 0
// through ModuleRvaInRange, so a zero entry in the list would admit every one
// of them at once.
void ZeroSlotsNeverMatch() {
    void* outside[] = { reinterpret_cast<void*>(kBase - 0x100) };
    CHECK_MSG(MatchDrawingCaller(outside, 1) == 0, "an address below the module does not match");
    void* above[] = { reinterpret_cast<void*>(kBase + kSize + 0x100) };
    CHECK_MSG(MatchDrawingCaller(above, 1) == 0, "an address above the module does not match");
}

// An address that would collide with a listed RVA if the base were subtracted
// blindly. This is what ModuleRvaInRange exists to stop.
void ForeignModuleCollisionIsRejected() {
    void* frames[] = { reinterpret_cast<void*>(kBase + kSize + 0x1000) };
    CHECK_MSG(MatchDrawingCaller(frames, 1) == 0,
              "an address in another module does not match a listed RVA");
}

void EmptyStackMatchesNothing() {
    CHECK_MSG(MatchDrawingCaller(nullptr, 0) == 0, "no frames matches nothing");
}

}  // namespace

int main() {
    SetUp();
    ListedCallerIsMatched();
    UnlistedCallerIsRejected();
    TheMatchedRvaIsReported();
    ZeroSlotsNeverMatch();
    ForeignModuleCollisionIsRejected();
    EmptyStackMatchesNothing();
    return hol_test::Report();
}
