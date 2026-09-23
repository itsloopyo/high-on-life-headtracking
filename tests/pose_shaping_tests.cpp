// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// What the sights and the zoom do to the head pose.
//
// Rotation carries straight on through an aim; only the lean is eased out, on
// AdsFade. The fade's own timing is core's contract and is tested there; these
// are the cases this mod's wiring of it would break on.

#include <cmath>

#include "pose_shaping.h"
#include "test_harness.h"

namespace {

using hol_ht::AdsFade;
using hol_ht::ShapedPose;
using hol_ht::ShapePose;
using hol_ht::ZoomReference;

constexpr float kTol = 1e-5f;

ShapedPose MakePose(float yaw, float pitch, float roll, float x, float y, float z) {
    ShapedPose p;
    p.yaw = yaw; p.pitch = pitch; p.roll = roll;
    p.x = x; p.y = y; p.z = z;
    return p;
}

void CheckRotationUntouched(const ShapedPose& out, const ShapedPose& in) {
    CHECK_NEAR(out.yaw, in.yaw, kTol);
    CHECK_NEAR(out.pitch, in.pitch, kTol);
    CHECK_NEAR(out.roll, in.roll, kTol);
}

void TestHipFirePassesThePoseThroughUntouched() {
    AdsFade fade;
    const auto in = MakePose(-25.0f, 12.0f, 7.0f, 0.1f, -0.05f, -0.2f);
    const auto out = ShapePose(in, fade.Update(false, 1000), 1.0f);
    CheckRotationUntouched(out, in);
    CHECK_NEAR(out.x, in.x, kTol);
    CHECK_NEAR(out.y, in.y, kTol);
    CHECK_NEAR(out.z, in.z, kTol);
}

// Sights fully up: rotation, roll included, is absolute and unscaled, and the
// lean is gone.
void TestSightsUpKeepsRotationAndDropsTheLean() {
    AdsFade fade;
    fade.Update(false, 0);
    fade.Update(true, 0);
    const float scale = fade.Update(true, AdsFade::kLowerMs * 2);
    const auto in = MakePose(-25.0f, 12.0f, 7.0f, 0.1f, -0.05f, -0.2f);
    const auto out = ShapePose(in, scale, 1.0f);
    CheckRotationUntouched(out, in);
    CHECK_NEAR(out.x, 0.0f, kTol);
    CHECK_NEAR(out.y, 0.0f, kTol);
    CHECK_NEAR(out.z, 0.0f, kTol);
}

void TestMidTransitionScalesOnlyTheLean() {
    AdsFade fade;
    fade.Update(false, 0);
    fade.Update(true, 0);
    const float scale = fade.Update(true, AdsFade::kLowerMs / 2);
    CHECK(scale > 0.0f && scale < 1.0f);
    const auto in = MakePose(30.0f, -8.0f, -4.0f, 0.2f, 0.1f, -0.3f);
    const auto out = ShapePose(in, scale, 1.0f);
    CheckRotationUntouched(out, in);
    CHECK_NEAR(out.x, in.x * scale, kTol);
    CHECK_NEAR(out.y, in.y * scale, kTol);
    CHECK_NEAR(out.z, in.z * scale, kTol);
}

// A tap of the aim button releases while the lean is part way out. The reversal
// starts from where the transition is, or the lean steps.
void TestReversalContinuesFromWhereItWas() {
    AdsFade fade;
    fade.Update(false, 0);
    fade.Update(true, 0);
    const float part = fade.Update(true, AdsFade::kLowerMs / 2);
    const float back = fade.Update(false, AdsFade::kLowerMs / 2);
    CHECK_NEAR(back, part, 1e-3f);
    CHECK_NEAR(fade.Update(false, AdsFade::kLowerMs / 2 + AdsFade::kRaiseMs), 1.0f, kTol);
}

// Yaw, pitch and the lean scale with the zoom so they move the picture as far
// as they did un-zoomed. Roll rotates the picture by the same angle at every
// field of view, so it is left alone.
void TestZoomScalesYawPitchAndLeanButNotRoll() {
    const float zoom = 0.5f;
    const auto in = MakePose(20.0f, -10.0f, 15.0f, 0.2f, 0.1f, -0.3f);
    const auto out = ShapePose(in, 1.0f, zoom);
    constexpr float kDeg = 3.14159265358979323846f / 180.0f;
    CHECK_NEAR(std::tan(out.yaw * kDeg), std::tan(in.yaw * kDeg) * zoom, kTol);
    CHECK_NEAR(std::tan(out.pitch * kDeg), std::tan(in.pitch * kDeg) * zoom, kTol);
    CHECK_NEAR(out.roll, in.roll, kTol);
    CHECK_NEAR(out.x, in.x * zoom, kTol);
    CHECK_NEAR(out.y, in.y * zoom, kTol);
    CHECK_NEAR(out.z, in.z * zoom, kTol);
}

// The gate the doctrine sets: exactly 1 at the hip, by construction.
void TestZoomIsOneAtTheHip() {
    ZoomReference zoom;
    CHECK_NEAR(zoom.Update(115.4f, true), 1.0f, 0.0f);
    CHECK_NEAR(zoom.Update(90.0f, true), 1.0f, 0.0f);
}

// With the sights up the reference holds the hip FOV, and the factor is the
// tangent ratio of the two.
void TestZoomWithSightsUpIsTheTangentRatio() {
    ZoomReference zoom;
    zoom.Update(90.0f, true);
    const float factor = zoom.Update(60.0f, false);
    CHECK_NEAR(factor, std::tan(30.0f * 3.14159265358979f / 180.0f), kTol);
    CHECK_NEAR(zoom.BaseDeg(), 90.0f, 0.0f);
}

// An unreadable FOV, or no hip frame yet to measure against, is no
// compensation rather than a guessed one.
void TestZoomWithNothingToMeasureAgainstIsOne() {
    ZoomReference zoom;
    CHECK_NEAR(zoom.Update(60.0f, false), 1.0f, 0.0f);
    zoom.Update(90.0f, true);
    CHECK_NEAR(zoom.Update(0.0f, false), 1.0f, 0.0f);
    zoom.Reset();
    CHECK_NEAR(zoom.Update(60.0f, false), 1.0f, 0.0f);
}

// A hip frame replaces the reference outright, so a wide FOV seen once does not
// leave every later frame under-scaled.
void TestZoomReferenceIsNotAMaximum() {
    ZoomReference zoom;
    zoom.Update(120.0f, true);
    CHECK_NEAR(zoom.Update(90.0f, true), 1.0f, 0.0f);
    CHECK_NEAR(zoom.BaseDeg(), 90.0f, 0.0f);
}

}  // namespace

int main() {
    TestHipFirePassesThePoseThroughUntouched();
    TestSightsUpKeepsRotationAndDropsTheLean();
    TestMidTransitionScalesOnlyTheLean();
    TestReversalContinuesFromWhereItWas();
    TestZoomScalesYawPitchAndLeanButNotRoll();
    TestZoomIsOneAtTheHip();
    TestZoomWithSightsUpIsTheTangentRatio();
    TestZoomWithNothingToMeasureAgainstIsOne();
    TestZoomReferenceIsNotAMaximum();

    return hol_test::Report();
}
