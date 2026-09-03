// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "xs_camera.h"

#include <atomic>
#include <cmath>
#include <cstdint>

#include "ads_gate.h"
#include "builds/build_registry.h"
#include "camera_boundary.h"
#include "frame_pose.h"
#include "logging.h"
#include "rva_hook.h"
#include "ue_types.h"
#include "view_hook.h"
#include "xs_widgets.h"

#include "cameraunlock/unreal/ue_math.h"
#include "cameraunlock/unreal/ue_runtime.h"

namespace hol_ht::xs_camera {

namespace {

namespace ue = ::cameraunlock::unreal;

using frame_pose::FramePose;
using ue4::FRotator3f;

// void USceneCaptureComponent2D::SetCameraView(const FMinimalViewInfo& View).
// The struct is 0x890 bytes and only two of its fields are touched here, so it
// arrives as an opaque pointer and those are addressed through the profile's
// FMinimalViewInfo layout - the same layout the live FOV is read through on the
// player's own view.
using SetCameraView_t = void(__fastcall*)(void* self, void* view);

SetCameraView_t g_orig = nullptr;

// The last frame the retro game's camera was asked for. Written on the game
// thread by the hook below, read on the game thread by the player's view query.
std::atomic<std::uint64_t> g_frame{~0ull};

// The frame's two rotations, held between composing the view and placing the
// mark - which now happen either side of the engine's own call.
ue::FRotator g_clean{}, g_composed{};

// The capture's render target is sized by AORXSPlayerCharacter::UpdateSceneCapture
// to (viewport height * 16/9) x (viewport height), so its aspect is fixed at
// 16:9 whatever the display is.
constexpr float kCaptureAspect = 16.0f / 9.0f;

// The two diagonal terms of the projection the capture will render with:
// ndcX = (right / depth) * m00, ndcY = (up / depth) * m11.
struct Projection {
    float m00 = 0.0f;
    float m11 = 0.0f;
};

// Read them off the component rather than deriving them from a FOV.
//
// A scene capture can carry a projection matrix of its own, and when it does,
// FOVAngle is decoration - every formula built on it is then wrong by whatever
// ratio the two disagree by, silently and by a constant, which is exactly what
// an overcompensating reticle looks like. So the engine is asked what it is
// about to use. With no custom matrix this rebuilds UE's own
// BuildProjectionMatrix for scene captures: M[0][0] = 1/tan(FOV/2), and
// M[1][1] = M[0][0] * aspect.
bool ReadProjection(void* capture, Projection& out) {
    const std::uintptr_t c = reinterpret_cast<std::uintptr_t>(capture);
    if (c == 0) return false;

    std::uint32_t custom = 0;
    if (ue::SafeReadU32(c + Offsets().kCaptureUseCustomProjectionOffset, custom) &&
        (custom & 0xffu) != 0) {
        const std::uintptr_t m = c + Offsets().kCaptureCustomProjectionOffset;
        if (!ue::SafeReadFloat(m + 0x00, out.m00) || !ue::SafeReadFloat(m + 0x14, out.m11))
            return false;
        return out.m00 != 0.0f && out.m11 != 0.0f;
    }

    float fovDegrees = 0.0f;
    if (!ue::SafeReadFloat(c + Offsets().kCaptureFovAngleOffset, fovDegrees)) return false;
    if (!(fovDegrees > 1.0f && fovDegrees < 179.0f)) return false;
    const float tanHalf = std::tan(fovDegrees * 0.5f * 3.14159265358979f / 180.0f);
    if (!(tanHalf > 0.0f)) return false;
    out.m00 = 1.0f / tanHalf;
    out.m11 = out.m00 * kCaptureAspect;
    return true;
}

// Said once, with every term the mark's position is computed from. Three
// attempts at this reticle were three different guesses at one of these; the
// line exists so that a fourth is arithmetic instead of a fourth guess.
void LogProjectionOnce(void* capture, const Projection& proj) {
    static std::atomic<bool> s_logged{false};
    if (s_logged.exchange(true)) return;
    const std::uintptr_t c = reinterpret_cast<std::uintptr_t>(capture);
    float fov = 0.0f;
    std::uint32_t custom = 0, projType = 0;
    ue::SafeReadFloat(c + Offsets().kCaptureFovAngleOffset, fov);
    ue::SafeReadU32(c + Offsets().kCaptureUseCustomProjectionOffset, custom);
    ue::SafeReadU32(c + Offsets().kCaptureProjectionTypeOffset, projType);
    Log::Line("xs-camera: capture projection m00=%.4f m11=%.4f (FOVAngle=%.2f, "
              "customMatrix=%u, projectionType=%u)",
              proj.m00, proj.m11, fov, custom & 0xffu, projType & 0xffu);
}

// Put the retro crosshair where the retro game's shot is going.
//
// The shot goes along the CLEAN camera's forward - the retro pawn's own control
// rotation, which nothing here writes to - and the frame is drawn from the
// tracked one, so the mark belongs where the clean forward direction lands in
// the tracked camera's frame. That is a DIRECTION, not a point, and it is why
// the capture takes rotation only: with no translation between the eye that
// shoots and the eye that draws, one direction projects to one pixel at EVERY
// range, so the mark cannot be right at one distance and wrong either side of
// it. There is no aim raycast to borrow here - the game never projects this
// crosshair - and a fixed convergence distance is exactly the fault the reticle
// doctrine names.
void MoveCrosshairFromCapture(void* capture);

void MoveCrosshair(const ue::FRotator& clean, const ue::FRotator& tracked,
                  const Projection& proj) {
    const ue::FQuat4d cleanQ = ue::QuatFromEulerDeg(clean.Pitch, clean.Yaw, clean.Roll);
    const ue::FQuat4d trackedQ = ue::QuatFromEulerDeg(tracked.Pitch, tracked.Yaw, tracked.Roll);
    // Unit quaternion, so the conjugate is the inverse.
    const ue::FQuat4d trackedInv{-trackedQ.X, -trackedQ.Y, -trackedQ.Z, trackedQ.W};
    const ue::FQuat4d relative = ue::QuatMul(trackedInv, cleanQ);

    // The clean-aim direction in the tracked camera's frame, in UE camera axes:
    // X forward, Y right, Z up. This is relative * (1,0,0), the first column of
    // its rotation matrix - the same decomposition core's aim projections use.
    const double depth = 1.0 - 2.0 * (relative.Y * relative.Y + relative.Z * relative.Z);
    const double right = 2.0 * (relative.X * relative.Y + relative.W * relative.Z);
    const double up    = 2.0 * (relative.X * relative.Z - relative.W * relative.Y);
    if (depth <= 0.01) {
        // The aim is behind the tracked view. Put the mark back rather than
        // pinning it to an edge - it is not on screen, and a mark parked at the
        // edge reads as the shot going there.
        xs_widgets::Move(0.0f, 0.0f);
        return;
    }

    // The engine's own projection terms, so no FOV model sits between this and
    // what the capture actually renders.
    const float ndcX = static_cast<float>(right / depth) * proj.m00;
    const float ndcY = static_cast<float>(up / depth) * proj.m11;

    // The picture's extent comes from the HUD's own Slate geometry, not from the
    // backbuffer. Deriving it from the render target's pixel size and the
    // viewport was the overcompensation: Slate's absolute space is not the
    // backbuffer, the window's scaling sits between the two, and nothing in this
    // mod could see that factor - so every move came out a multiple of what was
    // asked for.
    float pictureHalfWidth = 0.0f, pictureHalfHeight = 0.0f;
    if (!xs_widgets::PictureHalfExtent(pictureHalfWidth, pictureHalfHeight)) return;

    // Screen y runs down the frame and normalised y runs up it.
    xs_widgets::Move(ndcX * pictureHalfWidth, -ndcY * pictureHalfHeight);
}

// Compose the frame's head pose onto the view the capture is about to render
// with, and move the crosshair to match.
//
// The pose is the one the last player-view query settled on, not this frame's:
// the capture's camera is built during the controller's tick and the player's
// view is not asked for until the frame is drawn, so this runs first. That is a
// frame of latency and nothing more - both cameras are still driven from a
// single decision about what the head is doing, which is the property
// frame_pose.h exists to keep.
bool ApplyPose(void* capture, void* view) {
    const FramePose& pose = frame_pose::Latest();

    // Rotation only. Position would move the eye that draws away from the eye
    // that shoots, and then no single pixel is the shot at every range - see
    // MoveCrosshair. Head lean stays on everywhere else in the game.
    if (!PoseApplies(pose.Verdict) || !pose.HasRotation) {
        xs_widgets::Move(0.0f, 0.0f);
        return false;
    }

    (void)capture;

    auto* rotation = reinterpret_cast<FRotator3f*>(
        static_cast<std::uint8_t*>(view) + Offsets().MinimalViewInfoLayout.kRotationStride);

    const ue::FRotator clean = ue4::ToCore(*rotation);
    ue::FRotator composed = clean;
    camera_boundary::ApplyHeadPose(composed, pose.Yaw, pose.Pitch, pose.Roll,
                                   view_hook::WorldSpaceYaw());
    ue4::FromCore(composed, *rotation);

    g_clean = clean;
    g_composed = composed;
    return true;
}

// The crosshair, placed from the projection the capture is left holding.
void MoveCrosshairFromCapture(void* capture) {
    Projection proj;
    if (!ReadProjection(capture, proj)) return;
    LogProjectionOnce(capture, proj);
    MoveCrosshair(g_clean, g_composed, proj);
}

void __fastcall SetCameraView_Hook(void* self, void* view) {
    g_frame.store(view_hook::CurrentFrameNumber(), std::memory_order_release);

    static std::atomic<bool> s_seen{false};
    if (!s_seen.exchange(true)) {
        Log::Line("xs-camera: the retro game at the start is on screen - its own camera takes "
                  "the head pose and its crosshair moves with it");
    }

    // Head tracking runs here ONLY while the crosshair can be moved. That
    // crosshair is a fixed mark at the middle of the picture, correct only while
    // the camera looks where the retro gun aims, so moving the camera without
    // moving the mark would put the mark where the rounds are not going - the
    // one thing this mod must never ship.
    // The rotation must go in BEFORE the engine consumes the view.
    const bool moved = (view != nullptr && xs_widgets::EnsureWidget()) && ApplyPose(self, view);

    g_orig(self, view);

    // The crosshair is placed AFTER, from the projection the component ends the
    // call holding. Reading it beforehand was wrong: FMinimalViewInfo::FOV says
    // 115.37 there, while the picture measures as ~140 degrees across, and that
    // 1.764x is precisely the overcompensation. Whatever sets the final value -
    // SetCameraView itself, or FinalizePOVSceneCapture after it - the last
    // writer wins, so the last state is the one to ask.
    if (moved) MoveCrosshairFromCapture(self);

}

}  // namespace

bool OwnsPose(std::uint64_t frame) {
    const std::uint64_t last = g_frame.load(std::memory_order_acquire);
    if (last == ~0ull) return false;
    // The capture's camera is built during the tick and the player's view is
    // asked for when the frame is drawn, so `last` is this frame's number by the
    // time the view query reads it. The frame either side of that is accepted so
    // a build that updates the capture after the draw is covered too; the cost
    // is one extra clean frame as the section ends, in which the head has not
    // moved a degree.
    return frame == last || frame == last + 1;
}

bool Install() {
    const std::uintptr_t rva = Offsets().kSceneCaptureSetCameraViewRva;
    if (rva == 0) {
        Log::Line("xs-camera: this build profile has no SetCameraView RVA - the retro game "
                  "at the start goes unrecognised, so the player's view head-tracks through "
                  "it and slides its picture around the frame");
        return false;
    }

    if (!InstallRvaHook("xs-camera:", "SetCameraView", rva,
                        reinterpret_cast<void*>(&SetCameraView_Hook),
                        reinterpret_cast<void**>(&g_orig)))
        return false;

    xs_widgets::Install();

    Log::Line("xs-camera: the retro game's camera hooked at RVA 0x%08llx - the head pose "
              "moves what its screen draws, and its own crosshair moves with it",
        static_cast<unsigned long long>(rva));
    return true;
}

}  // namespace hol_ht::xs_camera
