// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// What the sights do to the head pose.
//
// The entry pose is what makes the tracked ADS modes swing onto the aim and then
// keep tracking from there, and it is the piece of the shared ADS module this
// mod feeds directly out of the frame-pose walk in frame_pose.cpp - engine
// degrees and metres, straight from the camera boundary. Its rules are pinned at
// the source too (cameraunlock-core, cpp/tests/ads_tests.cpp); these are the
// cases this mod would break on, and they cost nothing to run.
//
// The cycle strings and the transition timings are NOT retested here. They are a
// fleet-wide contract owned by the core, and a copy of them in every mod is a
// second place for them to drift.

#include "ads.h"
#include "test_harness.h"

namespace {

using hol_ht::AdsEntryPose;
using hol_ht::AdsFade;
using hol_ht::AdsMode;
using hol_ht::BlendAdsPose;

AdsEntryPose::Pose MakePose(float pitch, float yaw, float roll,
                            float x = 0.0f, float y = 0.0f, float z = 0.0f) {
    AdsEntryPose::Pose p;
    p.pitch = pitch; p.yaw = yaw; p.roll = roll;
    p.x = x; p.y = y; p.z = z;
    return p;
}

// Hip fire is untouched: whatever the tracker says reaches the camera.
void TestHipFirePassesThrough() {
    AdsEntryPose entry;
    const auto out = entry.Relative(false, true, MakePose(5.0f, -12.0f, 3.0f, 1.0f, 2.0f, 3.0f));
    CHECK_NEAR(out.pitch, 5.0f, 1e-6f);
    CHECK_NEAR(out.yaw, -12.0f, 1e-6f);
    CHECK_NEAR(out.roll, 3.0f, 1e-6f);
    CHECK_NEAR(out.x, 1.0f, 1e-6f);
    CHECK(!entry.HasEntry());
}

// The entry frame is identity, which is what puts the view on the point the
// crosshair was marking rather than wherever the head happens to be.
void TestEntryFrameIsIdentity() {
    AdsEntryPose entry;
    const auto out = entry.Relative(true, true, MakePose(20.0f, -35.0f, 0.0f, 4.0f, 5.0f, 6.0f));
    CHECK(entry.HasEntry());
    CHECK_NEAR(out.pitch, 0.0f, 1e-6f);
    CHECK_NEAR(out.yaw, 0.0f, 1e-6f);
    CHECK_NEAR(out.x, 0.0f, 1e-6f);
    CHECK_NEAR(out.y, 0.0f, 1e-6f);
    CHECK_NEAR(out.z, 0.0f, 1e-6f);
}

// Roll moves no aim point, so zeroing it would yank a head tilt the player is
// actively holding back to level and lean it in again as they move: two horizon
// jolts per aim, buying nothing.
void TestRollStaysAbsolute() {
    AdsEntryPose entry;
    CHECK_NEAR(entry.Relative(true, true, MakePose(0.0f, 0.0f, 14.0f)).roll, 14.0f, 1e-6f);
    CHECK_NEAR(entry.Relative(true, true, MakePose(0.0f, 0.0f, -6.0f)).roll, -6.0f, 1e-6f);
}

// Yaw arrives wrapped into -180..180, so a plain subtraction reads a 10 degree
// move across the seam as -350 and whips the view a full turn the wrong way.
void TestYawCrossesTheSeamTheShortWay() {
    AdsEntryPose entry;
    entry.Relative(true, true, MakePose(0.0f, 175.0f, 0.0f));
    CHECK_NEAR(entry.Relative(true, true, MakePose(0.0f, -175.0f, 0.0f)).yaw, 10.0f, 1e-4f);

    AdsEntryPose back;
    back.Relative(true, true, MakePose(0.0f, -175.0f, 0.0f));
    CHECK_NEAR(back.Relative(true, true, MakePose(0.0f, 175.0f, 0.0f)).yaw, -10.0f, 1e-4f);
}

// Pitch is bounded by the tracker's own asin and cannot wrap, so it stays a
// plain difference - and the lean goes relative with it.
void TestPitchAndPositionAreRelative() {
    AdsEntryPose entry;
    entry.Relative(true, true, MakePose(10.0f, 0.0f, 0.0f, 3.0f, -1.0f, 2.0f));
    const auto out = entry.Relative(true, true, MakePose(-5.0f, 0.0f, 0.0f, 4.0f, -3.0f, 2.5f));
    CHECK_NEAR(out.pitch, -15.0f, 1e-6f);
    CHECK_NEAR(out.x, 1.0f, 1e-6f);
    CHECK_NEAR(out.y, -2.0f, 1e-6f);
    CHECK_NEAR(out.z, 0.5f, 1e-6f);
}

// The path that hits this: aim, open a menu, move your head, close it with the
// sights still up. The interpolators publish nothing until a fresh packet lands,
// so capturing on a dead frame would freeze a pre-suppression pose and hold the
// whole aim at that offset.
void TestCaptureWaitsForALiveRotation() {
    AdsEntryPose entry;
    entry.Relative(true, false, MakePose(9.0f, 9.0f, 0.0f));
    CHECK(!entry.HasEntry());
    const auto out = entry.Relative(true, true, MakePose(30.0f, -20.0f, 0.0f));
    CHECK(entry.HasEntry());
    CHECK_NEAR(out.pitch, 0.0f, 1e-6f);
    CHECK_NEAR(out.yaw, 0.0f, 1e-6f);
}

// Lowering the weapon drops it, so the next aim re-enters from wherever the head
// is then rather than from a pose two firefights old.
void TestLoweringTheWeaponDropsTheEntry() {
    AdsEntryPose entry;
    entry.Relative(true, true, MakePose(10.0f, 40.0f, 0.0f));
    const auto down = entry.Relative(false, true, MakePose(12.0f, 45.0f, 0.0f));
    CHECK(!entry.HasEntry());
    CHECK_NEAR(down.yaw, 45.0f, 1e-6f);
    CHECK_NEAR(entry.Relative(true, true, MakePose(12.0f, 45.0f, 0.0f)).yaw, 0.0f, 1e-6f);
}

// At the hip every mode hands the camera the head pose whole.
void TestBlendHipIsTheHeadPose() {
    const auto absolute = MakePose(5.0f, -12.0f, 3.0f, 1.0f, 2.0f, 3.0f);
    const auto relative = MakePose(0.0f, 0.0f, 3.0f);
    for (const AdsMode mode : { AdsMode::Paused, AdsMode::Tracked }) {
        const auto out = BlendAdsPose(mode, 1.0f, absolute, relative);
        CHECK_NEAR(out.pitch, 5.0f, 1e-6f);
        CHECK_NEAR(out.yaw, -12.0f, 1e-6f);
        CHECK_NEAR(out.roll, 3.0f, 1e-6f);
        CHECK_NEAR(out.z, 3.0f, 1e-6f);
    }
}

// Sights fully up in `paused`: the view is the game's again, apart from the tilt
// the player is holding.
void TestBlendPausedKeepsRollAndDropsTheRest() {
    const auto absolute = MakePose(5.0f, -12.0f, 3.0f, 1.0f, 2.0f, 3.0f);
    const auto out = BlendAdsPose(AdsMode::Paused, 0.0f, absolute, MakePose(0.0f, 0.0f, 3.0f));
    CHECK_NEAR(out.pitch, 0.0f, 1e-6f);
    CHECK_NEAR(out.yaw, 0.0f, 1e-6f);
    CHECK_NEAR(out.x, 0.0f, 1e-6f);
    CHECK_NEAR(out.y, 0.0f, 1e-6f);
    CHECK_NEAR(out.z, 0.0f, 1e-6f);
    CHECK_NEAR(out.roll, 3.0f, 1e-6f);
}

// `tracked` lands on the entry-relative pose, so head tracking carries on from
// the aim rather than from centre.
void TestBlendTrackedLandsOnTheEntryRelativePose() {
    const auto absolute = MakePose(5.0f, -12.0f, 3.0f, 1.0f, 2.0f, 3.0f);
    const auto relative = MakePose(2.0f, -4.0f, 3.0f, 0.5f, 0.5f, 1.0f);
    const auto out = BlendAdsPose(AdsMode::Tracked, 0.0f, absolute, relative);
    CHECK_NEAR(out.pitch, 2.0f, 1e-6f);
    CHECK_NEAR(out.yaw, -4.0f, 1e-6f);
    CHECK_NEAR(out.x, 0.5f, 1e-6f);
    CHECK_NEAR(out.z, 1.0f, 1e-6f);
    CHECK_NEAR(out.roll, 3.0f, 1e-6f);
}

// The entry is a fade, not a switch: raising the sights eases the pose off over
// AdsFade::kLowerMs rather than cutting it in one frame.
void TestFadeEasesRatherThanCutting() {
    AdsFade fade;
    CHECK_NEAR(fade.Update(false, 1000), 1.0f, 1e-6f);
    CHECK_NEAR(fade.Update(true, 1000), 1.0f, 1e-6f);
    const float part = fade.Update(true, 1000 + AdsFade::kLowerMs / 2);
    CHECK(part > 0.0f && part < 1.0f);
    CHECK_NEAR(fade.Update(true, 1000 + AdsFade::kLowerMs * 2), 0.0f, 1e-6f);
}

// A tap of the aim button releases while the pose is part way off. The reversal
// has to start from where the transition is, or the view steps by however far
// the interrupted leg had travelled.
void TestFadeReversalStartsWhereItIs() {
    AdsFade fade;
    fade.Update(false, 0);
    fade.Update(true, 0);
    const float part = fade.Update(true, AdsFade::kLowerMs / 2);
    CHECK(part > 0.0f && part < 1.0f);
    // The frame the aim is released on carries on from `part` rather than
    // jumping to either end.
    const float back = fade.Update(false, AdsFade::kLowerMs / 2);
    CHECK_NEAR(back, part, 1e-3f);
    CHECK_NEAR(fade.Update(false, AdsFade::kLowerMs / 2 + AdsFade::kRaiseMs), 1.0f, 1e-6f);
}

}  // namespace

int main() {
    TestHipFirePassesThrough();
    TestEntryFrameIsIdentity();
    TestRollStaysAbsolute();
    TestYawCrossesTheSeamTheShortWay();
    TestPitchAndPositionAreRelative();
    TestCaptureWaitsForALiveRotation();
    TestLoweringTheWeaponDropsTheEntry();
    TestBlendHipIsTheHeadPose();
    TestBlendPausedKeepsRollAndDropsTheRest();
    TestBlendTrackedLandsOnTheEntryRelativePose();
    TestFadeEasesRatherThanCutting();
    TestFadeReversalStartsWhereItIs();

    return hol_test::Report();
}
