// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Behaviour locks for the tracker-to-Unreal camera boundary, the one piece of
// the camera path that is a pure function of its arguments.
//
// The numbers here are not derived from first principles - they come from
// still-wakes-the-deep-headtracking, the ancestor this boundary is a port of,
// where they were verified in a running game. Recording them means a later
// change to signs, axis mapping or composition order cannot pass quietly. A
// failure means the view or the aim decoupling has moved.

#include <cmath>

#include "camera_boundary.h"
#include "test_harness.h"

namespace {

namespace ue = ::cameraunlock::unreal;
using hol_ht::camera_boundary::ApplyHeadPose;
using hol_ht::camera_boundary::PositionOffset;

// The position offsets come in as floats, so their exact decimal value is
// already a few parts in 10^7 off before any arithmetic happens; those checks
// pass the looser tolerance. The rotations are doubles throughout.
constexpr double kAngleTolerance = 1e-6;
constexpr double kOffsetToleranceCm = 1e-4;

#define CHECK_ANGLE(actual, expected, what) \
    CHECK_NEAR_MSG((actual), (expected), kAngleTolerance, (what))
#define CHECK_CM(actual, expected, what) \
    CHECK_NEAR_MSG((actual), (expected), kOffsetToleranceCm, (what))

// ---- rotation -------------------------------------------------------------

void WorldYawAddsPoseAndNegatesRoll() {
    ue::FRotator r{10.0, 20.0, 30.0};
    ApplyHeadPose(r, 5.0, 3.0, 7.0, /*worldSpaceYaw=*/true);
    CHECK_ANGLE(r.Pitch, 13.0, "world yaw: pitch adds");
    CHECK_ANGLE(r.Yaw,   25.0, "world yaw: yaw adds");
    CHECK_ANGLE(r.Roll,  23.0, "world yaw: roll subtracts");
}

void ZeroPoseIsIdentityInBothModes() {
    ue::FRotator world{12.0, -34.0, 5.0};
    ApplyHeadPose(world, 0.0, 0.0, 0.0, true);
    CHECK_ANGLE(world.Pitch, 12.0, "world yaw: zero pose leaves pitch");
    CHECK_ANGLE(world.Yaw,  -34.0, "world yaw: zero pose leaves yaw");
    CHECK_ANGLE(world.Roll,   5.0, "world yaw: zero pose leaves roll");

    ue::FRotator local{12.0, -34.0, 5.0};
    ApplyHeadPose(local, 0.0, 0.0, 0.0, false);
    CHECK_ANGLE(local.Pitch, 12.0, "local yaw: zero pose leaves pitch");
    CHECK_ANGLE(local.Yaw,  -34.0, "local yaw: zero pose leaves yaw");
    CHECK_ANGLE(local.Roll,   5.0, "local yaw: zero pose leaves roll");
}

// From a level camera the two modes agree: camera-local up is world up, so the
// quaternion compose and the FRotator addition describe the same rotation. They
// only part company once the base rotation is pitched, which is the whole reason
// the yaw-mode toggle exists.
void ModesAgreeOnLevelBaseAndDivergeWhenPitched() {
    ue::FRotator world{0.0, 40.0, 0.0};
    ue::FRotator local = world;
    ApplyHeadPose(world, 15.0, 10.0, 0.0, true);
    ApplyHeadPose(local, 15.0, 10.0, 0.0, false);
    CHECK_ANGLE(local.Pitch, world.Pitch, "level base: pitch agrees across modes");
    CHECK_ANGLE(local.Yaw,   world.Yaw,   "level base: yaw agrees across modes");
    CHECK_ANGLE(local.Roll,  world.Roll,  "level base: roll agrees across modes");

    ue::FRotator pitchedWorld{-50.0, 40.0, 0.0};
    ue::FRotator pitchedLocal = pitchedWorld;
    ApplyHeadPose(pitchedWorld, 30.0, 0.0, 0.0, true);
    ApplyHeadPose(pitchedLocal, 30.0, 0.0, 0.0, false);
    CHECK_MSG(std::fabs(pitchedLocal.Roll - pitchedWorld.Roll) > 1.0,
              "pitched base: camera-local yaw leans the horizon, world yaw does not");
    CHECK_ANGLE(pitchedWorld.Roll, 0.0, "pitched base: world yaw keeps the horizon level");
}

// The composition the ancestor renders with, recorded angle for angle.
void LocalYawCompositionIsUnchanged() {
    ue::FRotator r{-20.0, 35.0, 4.0};
    ApplyHeadPose(r, 12.0, 8.0, 6.0, false);
    CHECK_ANGLE(r.Pitch, -12.375926322, "local yaw: pitch");
    CHECK_ANGLE(r.Yaw,    47.721496771, "local yaw: yaw");
    CHECK_ANGLE(r.Roll,   -6.410190882, "local yaw: roll");
}

// ---- position -------------------------------------------------------------

// With the camera at the world origin rotation, the three tracker axes land on
// the engine axes the mod's sign notes describe: -z is the forward lean, x is
// mirrored, y is up. Metres in, centimetres out.
void PositionOffsetMapsTrackerAxesToUnreal() {
    const ue::FQuat4d identity{0.0, 0.0, 0.0, 1.0};

    const ue::FVector surge = PositionOffset(identity, 0.0f, 0.0f, -0.40f);
    CHECK_CM(surge.X, 40.0, "surge: negative tracker z leans forward, in cm");
    CHECK_CM(surge.Y,  0.0, "surge: no sway");
    CHECK_CM(surge.Z,  0.0, "surge: no heave");

    const ue::FVector sway = PositionOffset(identity, 0.30f, 0.0f, 0.0f);
    CHECK_CM(sway.X,   0.0, "sway: no surge");
    CHECK_CM(sway.Y, -30.0, "sway: tracker x is mirrored against engine right");
    CHECK_CM(sway.Z,   0.0, "sway: no heave");

    const ue::FVector heave = PositionOffset(identity, 0.0f, 0.20f, 0.0f);
    CHECK_CM(heave.X,  0.0, "heave: no surge");
    CHECK_CM(heave.Y,  0.0, "heave: no sway");
    CHECK_CM(heave.Z, 20.0, "heave: tracker y maps straight to engine up");
}

// The offset is built in the CLEAN camera frame, so it follows the body: yaw the
// camera 90 degrees and a forward lean has to come out along engine +Y.
void PositionOffsetFollowsTheCameraBasis() {
    const ue::FQuat4d yawed = ue::QuatFromEulerDeg(0.0, 90.0, 0.0);
    const ue::FVector surge = PositionOffset(yawed, 0.0f, 0.0f, -0.40f);
    CHECK_CM(surge.X,  0.0, "yawed surge: nothing left on engine X");
    CHECK_CM(surge.Y, 40.0, "yawed surge: forward lean follows the camera");
    CHECK_CM(surge.Z,  0.0, "yawed surge: nothing on engine Z");
}

}  // namespace

int main() {
    WorldYawAddsPoseAndNegatesRoll();
    ZeroPoseIsIdentityInBothModes();
    ModesAgreeOnLevelBaseAndDivergeWhenPitched();
    LocalYawCompositionIsUnchanged();
    PositionOffsetMapsTrackerAxesToUnreal();
    PositionOffsetFollowsTheCameraBasis();

    return hol_test::Report();
}
