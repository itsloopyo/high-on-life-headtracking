// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// The verdict walk: whether the head pose reaches the view, and what the frame
// reports about the sights while it decides.
//
// ADS is tested last in that walk, so a menu or a cinematic still names its own
// reason when both are true at once, and no early return may leave the sights
// flag set - a stale flag through a menu would hold the pose to the aim it was
// blended into against a weapon that is not raised.

#include "ads_gate.h"
#include "test_harness.h"

namespace {

using hol_ht::AdsMode;
using hol_ht::DecideTracking;
using hol_ht::PoseApplies;
using hol_ht::TrackingVerdict;
using hol_ht::game_state::Phase;

// `paused` closes the gate on the aim and still reports the sights: the gate
// says whether tracking applies, the flag says what the weapon is doing.
void TestPausedClosesTheGateAndStillReportsTheSights() {
    const auto s = DecideTracking(Phase::Gameplay, true, true, true, AdsMode::Paused);
    CHECK(s.verdict == TrackingVerdict::AdsSuspended);
    CHECK(s.aiming);
    // A pose still reaches the camera, because suspending is an ease-out rather
    // than a switch. Dropping it on the falling edge would throw the smoothing
    // state away and swing the view back through the head angle on the way out.
    CHECK(PoseApplies(s.verdict));
}

void TestTrackedStaysOpenThroughAnAim() {
    const auto s = DecideTracking(Phase::Gameplay, true, true, true, AdsMode::Tracked);
    CHECK(s.verdict == TrackingVerdict::Active);
    CHECK(s.aiming);
    CHECK(PoseApplies(s.verdict));
}

void TestHipFireIsActiveInEveryMode() {
    for (const AdsMode mode : { AdsMode::Paused, AdsMode::Tracked }) {
        const auto s = DecideTracking(Phase::Gameplay, true, true, false, mode);
        CHECK(s.verdict == TrackingVerdict::Active);
        CHECK(!s.aiming);
    }
}

// A menu, a cinematic or a controller this profile has no flags for outranks ADS
// in the reported reason, and clears the sights flag with it.
void TestSuppressionOutranksAdsAndClearsTheFlag() {
    for (const Phase phase : { Phase::Menu, Phase::Cinematic, Phase::UnknownClass }) {
        for (const AdsMode mode : { AdsMode::Paused, AdsMode::Tracked }) {
            const auto s = DecideTracking(phase, true, true, true, mode);
            CHECK(s.verdict == TrackingVerdict::NotGameplay);
            CHECK(!s.aiming);
            CHECK(!PoseApplies(s.verdict));
        }
    }
}

// The master toggle outranks everything, and reports its own reason.
void TestMasterToggleOutranksAds() {
    const auto s = DecideTracking(Phase::Gameplay, false, true, true, AdsMode::Tracked);
    CHECK(s.verdict == TrackingVerdict::Disabled);
    CHECK(!s.aiming);
    CHECK(!PoseApplies(s.verdict));
}

// No tracker is not an ADS verdict either, and it must not report the sights.
void TestNoTrackerReportsItsOwnReason() {
    const auto s = DecideTracking(Phase::Gameplay, true, false, true, AdsMode::Tracked);
    CHECK(s.verdict == TrackingVerdict::NoTracker);
    CHECK(!s.aiming);
    CHECK(!PoseApplies(s.verdict));
}

// The state is recomputed from the game every frame rather than latched on an
// edge, so an exit event that never arrives - an aim released while firing, a
// state machine that transitions without one - heals on the next frame instead
// of stranding the player in ADS behaviour.
void TestAdsHealsWithoutAnExitEdge() {
    const auto aimed = DecideTracking(Phase::Gameplay, true, true, true, AdsMode::Paused);
    CHECK(aimed.verdict == TrackingVerdict::AdsSuspended);
    const auto healed = DecideTracking(Phase::Gameplay, true, true, false, AdsMode::Paused);
    CHECK(healed.verdict == TrackingVerdict::Active);
    CHECK(!healed.aiming);
}

// A mode cycled mid-aim is read on the next frame's walk, so it lands on the aim
// that is already in progress rather than on the next one.
void TestCyclingMidAimChangesTheVerdict() {
    CHECK(DecideTracking(Phase::Gameplay, true, true, true, AdsMode::Paused).verdict
          == TrackingVerdict::AdsSuspended);
    CHECK(DecideTracking(Phase::Gameplay, true, true, true, AdsMode::Tracked).verdict
          == TrackingVerdict::Active);
}

}  // namespace

int main() {
    TestPausedClosesTheGateAndStillReportsTheSights();
    TestTrackedStaysOpenThroughAnAim();
    TestHipFireIsActiveInEveryMode();
    TestSuppressionOutranksAdsAndClearsTheFlag();
    TestMasterToggleOutranksAds();
    TestNoTrackerReportsItsOwnReason();
    TestAdsHealsWithoutAnExitEdge();
    TestCyclingMidAimChangesTheVerdict();

    return hol_test::Report();
}
