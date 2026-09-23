// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cmath>

#include "cameraunlock/ads/ads_fade.h"
#include "cameraunlock/camera/zoom_compensation.h"

// The two engine-boundary adjustments made to the head pose after the tracker
// and before the camera: the lean eased out while the sights are up, and the
// whole pose scaled to the zoom. Pure, so both run in a test with no game.
namespace hol_ht {

using cameraunlock::ads::AdsFade;

struct ShapedPose {
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

// `leanScale` is AdsFade's output: 1 at the hip, 0 with the sights up. It
// touches the lean and nothing else, because a lean moves the eye off the
// weapon's sight line and a rotation about the eye does not.
//
// `zoom` is the FOV zoom factor, 1 at the hip. Yaw, pitch and the lean
// translate the picture, so they scale with it; roll rotates the picture by the
// same angle at every field of view, so it does not.
inline ShapedPose ShapePose(const ShapedPose& in, float leanScale, float zoom) {
    ShapedPose out;
    out.yaw = cameraunlock::camera::ScaleAngleForZoom(in.yaw, zoom);
    out.pitch = cameraunlock::camera::ScaleAngleForZoom(in.pitch, zoom);
    out.roll = in.roll;
    out.x = in.x * leanScale * zoom;
    out.y = in.y * leanScale * zoom;
    out.z = in.z * leanScale * zoom;
    return out;
}

// The un-zoomed field of view a zoom is measured against.
//
// Both numbers come from the same place, FMinimalViewInfo::FOV as the player's
// view query leaves it: UE's horizontal FOV in degrees. The reference is that
// field's value on the last gameplay frame with the sights down and settled, so
// the factor is exactly 1 at the hip by construction, and the two tangents can
// never be of different axes or units.
//
// It is held, not tracked, for as long as the sights are up and until the lean
// has fully returned after they drop, which covers the game widening its FOV
// back out. A hip frame then replaces it outright, so nothing it saw once - a
// sprint kick, a wide cinematic - survives into the frames after.
class ZoomReference {
public:
    // `fovDeg` is this frame's FOV, or 0 when it could not be read.
    // `settledHip` is a gameplay frame with the sights down and the lean back.
    // Returns the zoom factor for this frame: 1 when there is nothing to
    // measure against.
    float Update(float fovDeg, bool settledHip) {
        if (!(fovDeg > 0.0f)) return 1.0f;
        if (settledHip) m_baseDeg = fovDeg;
        if (!(m_baseDeg > 0.0f)) return 1.0f;
        return cameraunlock::camera::FovZoomFactor(TanHalf(fovDeg), TanHalf(m_baseDeg));
    }

    // Menus, cinematics and loads: the next hip frame sets it afresh, so an FOV
    // changed in the options menu applies from the moment play resumes.
    void Reset() { m_baseDeg = 0.0f; }

    float BaseDeg() const { return m_baseDeg; }

    static float TanHalf(float fovDeg) {
        constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
        return std::tan(fovDeg * 0.5f * kDegToRad);
    }

private:
    float m_baseDeg = 0.0f;
};

}  // namespace hol_ht
