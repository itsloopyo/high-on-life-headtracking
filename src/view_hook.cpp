// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Inject the head pose into what is drawn, and nowhere else.
//
// GetPlayerViewPoint fires from many call sites per frame. Two gates decide
// which of them gets the pose written back:
//
//   1. the return address must be the call inside ULocalPlayer::GetViewPoint,
//      so interaction traces, the audio listener, AI perception and
//      replication all keep the clean mouse/pad rotation
//   2. a drawing consumer must be on the stack (presentation_gate.h), because
//      GetViewPoint also serves four world-to-screen projections that are
//      gameplay decisions
//
// Together those two ARE the look/aim decoupling: no save/restore sandwich is
// needed because the game simply never observes the delta.
//
// The reticle falls out of the same gate. AORPlayerCharacter::UpdateCrosshairLocation
// projects the weapon's own world aim point through this view every frame and
// writes the result to CrosshairLocation, so letting that consumer through
// means the crosshair is placed by the engine's own matrices, from the eye the
// frame is actually drawn from. There is no second projection in this mod that
// could disagree with the first.
//
// That gate is why this mod hooks nothing on the path between
// UpdateCrosshairLocation and here. A detour anywhere in that chain -
// ProjectWorldLocationToScreen was the one that shipped - puts a frame with no
// unwind data of the game's on the stack, RtlCaptureStackBackTrace stops there,
// and the crosshair's consumer never appears in the capture. The gate then
// leaves the view clean for the one query whose whole purpose is to be
// head-tracked, and the reticle sits welded to the middle of the screen while
// the world turns behind it. Which world point the crosshair is drawn FROM is
// changed instead, before the projection runs and off this stack, in
// aim_point.h.
//
// What the frame's pose IS lives in frame_pose.h; what the hook says about
// itself lives in view_diagnostics.h. This file is the trampoline, the two
// gates, and the write-back.

#include "view_hook.h"

#include <atomic>
#include <cstdint>

#include <intrin.h>

#include "ads.h"
#include "builds/build_registry.h"
#include "camera_boundary.h"
#include "frame_pose.h"
#include "game_state.h"
#include "logging.h"
#include "module_rva.h"
#include "presentation_gate.h"
#include "rva_hook.h"
#include "ue_types.h"
#include "view_diagnostics.h"
#include "xs_camera.h"

#include "cameraunlock/unreal/ue_math.h"
#include "cameraunlock/unreal/ue_runtime.h"

namespace hol_ht::view_hook {

namespace {

namespace ue = ::cameraunlock::unreal;

using frame_pose::FramePose;
using ue4::FRotator3f;
using ue4::FVector3f;

// APlayerController::GetPlayerViewPoint(self, &OutLocation, &OutRotation).
// FVector/FRotator are twelve bytes here: this is UE4, not UE5 with Large
// World Coordinates.
using GetPlayerViewPoint_t = void(__fastcall*)(void* self, FVector3f* outLocation,
                                               FRotator3f* outRotation);

Dependencies g_deps{};

std::atomic<bool> g_trackingEnabled{true};
std::atomic<bool> g_worldSpaceYaw{true};
std::atomic<AdsMode> g_adsMode{kDefaultAdsMode};

GetPlayerViewPoint_t g_origGetPlayerViewPoint = nullptr;
std::atomic<std::uint64_t> g_hookCallCount{0};
std::atomic<std::uint64_t> g_injectCount{0};

// Where the engine's game-thread frame number lives, resolved once at install.
const std::uint64_t* g_frameCounter = nullptr;

std::uint64_t CurrentFrame() {
    // Without GFrameCounter every call is its own "frame", which reintroduces
    // the intra-frame pose drift frame_pose.h describes. Install() logs that.
    if (!g_frameCounter) return g_hookCallCount.load(std::memory_order_relaxed);
    return *g_frameCounter;
}

// ---- clean camera, for the aim-point measurement -------------------------
std::atomic<bool>  g_cleanValid{false};
std::atomic<float> g_cleanX{0.0f}, g_cleanY{0.0f}, g_cleanZ{0.0f};

// The aiming flag is whatever Advance() last settled on, which on the drawing
// path is the previous frame's - the heartbeat is written before this frame's
// pose is walked, and a line every thirty seconds has no use for the difference.
view_diag::HeartbeatFields Heartbeat(std::uint64_t calls, game_state::Phase phase,
                                     bool drawing, AdsMode mode) {
    view_diag::HeartbeatFields fields{};
    fields.calls = calls;
    fields.injected = g_injectCount.load(std::memory_order_relaxed);
    fields.trackingEnabled = g_trackingEnabled.load(std::memory_order_relaxed);
    fields.phase = phase;
    fields.drawing = drawing;
    fields.receiver = g_deps.receiver;
    fields.worldSpaceYaw = g_worldSpaceYaw.load(std::memory_order_relaxed);
    fields.aiming = frame_pose::Latest().Aiming;
    fields.adsMode = mode;
    return fields;
}

// Compose the head pose onto the clean view the game just filled in. Runs only
// on a frame that cleared both gates and has a pose to apply.
void InjectPose(const FramePose& pose, FVector3f* outLocation, FRotator3f* outRotation) {
    const ue::FRotator clean = ue4::ToCore(*outRotation);

    if (pose.HasRotation) {
        ue::FRotator composed = clean;
        camera_boundary::ApplyHeadPose(composed, pose.Yaw, pose.Pitch, pose.Roll,
                                       g_worldSpaceYaw.load(std::memory_order_relaxed));
        ue4::FromCore(composed, *outRotation);
    }

    ue::FVector positionOffset{0.0, 0.0, 0.0};
    if (pose.HasPosition) {
        // Built from the CLEAN rotation, so head sway follows the body rather
        // than the head-rotated view.
        const ue::FQuat4d cleanQ = ue::QuatFromEulerDeg(clean.Pitch, clean.Yaw, clean.Roll);
        positionOffset = camera_boundary::PositionOffset(cleanQ, pose.OffX, pose.OffY, pose.OffZ);
        outLocation->X += static_cast<float>(positionOffset.X);
        outLocation->Y += static_cast<float>(positionOffset.Y);
        outLocation->Z += static_cast<float>(positionOffset.Z);
    }

    g_injectCount.fetch_add(1, std::memory_order_relaxed);
    view_diag::LogPoseDetail(clean, pose, *outRotation, positionOffset);
}

void __fastcall GetPlayerViewPoint_Hook(void* self, FVector3f* outLocation,
                                        FRotator3f* outRotation) {
    const std::uintptr_t retRva = ModuleRva(_ReturnAddress());

    g_origGetPlayerViewPoint(self, outLocation, outRotation);
    const std::uint64_t calls = g_hookCallCount.fetch_add(1, std::memory_order_relaxed) + 1;

    const auto controller = reinterpret_cast<std::uintptr_t>(self);

    // Cheapest test first, and nothing above it: everything below only ever
    // applies to the one call inside ULocalPlayer::GetViewPoint, and most calls
    // are not that. Classifying the controller is four fault-guarded reads, so
    // it waits until either the gate has passed or the heartbeat is actually due.
    if (retRva != Offsets().kViewPointCallerRva) {
        if (view_diag::HeartbeatDue(calls)) {
            view_diag::LogHeartbeat(Heartbeat(calls, game_state::Classify(controller), false,
                                              g_adsMode.load(std::memory_order_relaxed)));
        }
        return;
    }

    const game_state::Phase phase = game_state::Classify(controller);
    const AdsMode mode = g_adsMode.load(std::memory_order_relaxed);

    view_diag::ReadRenderFov(outLocation, outRotation);

    const bool drawing = presentation::CallerIsDrawing();
    if (!drawing) presentation::LogUnmatched();

    game_state::LogTransition(phase);
    if (view_diag::HeartbeatDue(calls))
        view_diag::LogHeartbeat(Heartbeat(calls, phase, drawing, mode));

    g_cleanX.store(outLocation->X, std::memory_order_relaxed);
    g_cleanY.store(outLocation->Y, std::memory_order_relaxed);
    g_cleanZ.store(outLocation->Z, std::memory_order_relaxed);
    g_cleanValid.store(true, std::memory_order_relaxed);

    // Advanced once per engine frame, whether or not this particular query is a
    // drawing one: the ADS transition has to run on every frame, and every
    // consumer in a frame has to see the same pose.
    const FramePose& pose = frame_pose::Advance(
        CurrentFrame(), controller, phase, *g_deps.session,
        g_trackingEnabled.load(std::memory_order_relaxed), mode);

    if (!drawing) return;

    // The retro game at the start draws itself through its own camera onto a
    // screen in the room (xs_camera.h). Head tracking steps aside for those
    // frames entirely - this view is only the room that screen stands in, and
    // moving it slides a flat picture around the frame.
    if (xs_camera::OwnsPose(CurrentFrame())) return;

    if (!PoseApplies(pose.Verdict)) return;
    if (!pose.HasRotation && !pose.HasPosition) return;

    InjectPose(pose, outLocation, outRotation);
}

}  // namespace

bool Install(const Dependencies& deps) {
    g_deps = deps;
    g_trackingEnabled.store(deps.config->enable_on_startup);
    g_worldSpaceYaw.store(deps.config->world_space_yaw);
    g_adsMode.store(deps.config->ads_mode);

    presentation::Init(Offsets().kPresentationCallerRvas.data(),
                       Offsets().kPresentationCallerRvas.size(),
                       Offsets().kPresentationStackFrames);

    if (Offsets().kGFrameCounterRva != 0) {
        g_frameCounter = reinterpret_cast<const std::uint64_t*>(
            ue::ModuleBase() + Offsets().kGFrameCounterRva);
    } else {
        Log::Line("WARNING: no GFrameCounter in this build profile - the head pose is "
                  "sampled per view query instead of per frame, so the crosshair "
                  "shimmers against the world.");
    }

    if (!InstallRvaHook("FATAL:", "GetPlayerViewPoint", Offsets().kGetPlayerViewPointRva,
                        reinterpret_cast<void*>(&GetPlayerViewPoint_Hook),
                        reinterpret_cast<void**>(&g_origGetPlayerViewPoint)))
        return false;

    Log::Line("GetPlayerViewPoint hooked at RVA 0x%08llx (drawing gate on caller 0x%08llx)",
        static_cast<unsigned long long>(Offsets().kGetPlayerViewPointRva),
        static_cast<unsigned long long>(Offsets().kViewPointCallerRva));

    // An extra hook the profile may or may not have an RVA for, which says so in
    // the log and leaves the rest of the mod working either way.
    xs_camera::Install();

    Log::Line("ads: %s (Insert or Ctrl+Shift+U cycles it)",
              AdsModeValue(g_adsMode.load(std::memory_order_relaxed)));
    return true;
}

bool TrackingEnabled() { return g_trackingEnabled.load(); }
void SetTrackingEnabled(bool enabled) { g_trackingEnabled.store(enabled); }

bool WorldSpaceYaw() { return g_worldSpaceYaw.load(); }
void SetWorldSpaceYaw(bool worldSpaceYaw) { g_worldSpaceYaw.store(worldSpaceYaw); }

AdsMode GetAdsMode() { return g_adsMode.load(std::memory_order_relaxed); }
void SetAdsMode(AdsMode mode) { g_adsMode.store(mode, std::memory_order_relaxed); }

std::uint64_t CurrentFrameNumber() { return CurrentFrame(); }

bool CleanCameraLocation(float& x, float& y, float& z) {
    if (!g_cleanValid.load(std::memory_order_relaxed)) return false;
    x = g_cleanX.load(std::memory_order_relaxed);
    y = g_cleanY.load(std::memory_order_relaxed);
    z = g_cleanZ.load(std::memory_order_relaxed);
    return true;
}

}  // namespace hol_ht::view_hook
