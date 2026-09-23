// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// The verdict walk: whether the head pose reaches the view, and what the frame
// reports about the sights while it decides.
//
// The sights never close the gate. They are reported so the lean can be eased
// out, and no early return may leave the flag set - a stale flag through a menu
// would hold the lean out against a weapon that is not raised.

#include <initializer_list>

#include "tracking_gate.h"
#include "test_harness.h"

namespace {

using hol_ht::DecideTracking;
using hol_ht::PoseApplies;
using hol_ht::TrackingVerdict;
using hol_ht::game_state::Phase;

void TestAimingKeepsTheGateOpenAndReportsTheSights() {
    const auto s = DecideTracking(Phase::Gameplay, true, true, true);
    CHECK(s.verdict == TrackingVerdict::Active);
    CHECK(s.aiming);
    CHECK(PoseApplies(s.verdict));
}

void TestHipFireIsActive() {
    const auto s = DecideTracking(Phase::Gameplay, true, true, false);
    CHECK(s.verdict == TrackingVerdict::Active);
    CHECK(!s.aiming);
}

// A menu, a cinematic or a controller this profile has no flags for reports its
// own reason, and clears the sights flag with it.
void TestSuppressionClearsTheFlag() {
    for (const Phase phase : { Phase::Menu, Phase::Cinematic, Phase::UnknownClass }) {
        const auto s = DecideTracking(phase, true, true, true);
        CHECK(s.verdict == TrackingVerdict::NotGameplay);
        CHECK(!s.aiming);
        CHECK(!PoseApplies(s.verdict));
    }
}

void TestMasterToggleOutranksEverything() {
    const auto s = DecideTracking(Phase::Gameplay, false, true, true);
    CHECK(s.verdict == TrackingVerdict::Disabled);
    CHECK(!s.aiming);
    CHECK(!PoseApplies(s.verdict));
}

void TestNoTrackerReportsItsOwnReason() {
    const auto s = DecideTracking(Phase::Gameplay, true, false, true);
    CHECK(s.verdict == TrackingVerdict::NoTracker);
    CHECK(!s.aiming);
    CHECK(!PoseApplies(s.verdict));
}

// The state is recomputed from the game every frame rather than latched on an
// edge, so an exit event that never arrives heals on the next frame.
void TestAdsHealsWithoutAnExitEdge() {
    CHECK(DecideTracking(Phase::Gameplay, true, true, true).aiming);
    CHECK(!DecideTracking(Phase::Gameplay, true, true, false).aiming);
}

}  // namespace

int main() {
    TestAimingKeepsTheGateOpenAndReportsTheSights();
    TestHipFireIsActive();
    TestSuppressionClearsTheFlag();
    TestMasterToggleOutranksEverything();
    TestNoTrackerReportsItsOwnReason();
    TestAdsHealsWithoutAnExitEdge();

    return hol_test::Report();
}
