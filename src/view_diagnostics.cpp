// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "view_diagnostics.h"

#include <atomic>

#include <windows.h>

#include "builds/build_registry.h"
#include "logging.h"

#include "cameraunlock/unreal/ue_runtime.h"

namespace hol_ht::view_diag {

namespace {

namespace ue = ::cameraunlock::unreal;

constexpr std::uint64_t kHeartbeatMs = 30000;
constexpr std::uint64_t kPoseDetailMs = 2000;
constexpr int kPoseDetailLines = 20;

// Outside this range the number is not a field of view, so the pointer pair the
// stride test accepted was not an FMinimalViewInfo after all.
constexpr float kMinPlausibleFov = 10.0f;
constexpr float kMaxPlausibleFov = 170.0f;

std::atomic<float> g_renderFov{0.0f};

// Port state alongside the data flag, because "udpData=NO" alone cannot tell a
// stalled tracker from a port another program still holds - and only the second
// one resolves itself.
const char* UdpPortState(const cameraunlock::UdpReceiver* receiver) {
    if (!receiver) return "none";
    if (receiver->IsRetrying()) return "waiting-for-port";
    return receiver->IsRunning() ? "listening" : "down";
}

}  // namespace

void ReadRenderFov(const ue4::FVector3f* outLocation, const ue4::FRotator3f* outRotation) {
    const auto& mvi = Offsets().MinimalViewInfoLayout;
    const auto locAddr = reinterpret_cast<std::uintptr_t>(outLocation);
    const auto rotAddr = reinterpret_cast<std::uintptr_t>(outRotation);
    if (rotAddr - locAddr != mvi.kRotationStride) return;

    float fov = 0.0f;
    if (!ue::SafeReadFloat(locAddr + mvi.kFovOffset, fov)) return;
    // Phrased as a range test rather than its negation so a NaN, which fails
    // every comparison, is rejected instead of stored.
    if (!(fov >= kMinPlausibleFov && fov <= kMaxPlausibleFov)) return;
    g_renderFov.store(fov, std::memory_order_relaxed);
}

bool HeartbeatDue(std::uint64_t calls) {
    static std::atomic<std::uint64_t> s_lastTick{0};
    const std::uint64_t now = GetTickCount64();
    if (calls != 1 && (now - s_lastTick.load(std::memory_order_relaxed)) < kHeartbeatMs)
        return false;
    s_lastTick.store(now, std::memory_order_relaxed);
    return true;
}

void LogHeartbeat(const HeartbeatFields& fields) {
    float hy = 0, hp = 0, hr = 0;
    const bool data = fields.receiver && fields.receiver->GetRotation(hy, hp, hr);

    Log::Line("heartbeat calls=%llu injected=%llu enabled=%s state=%s drawing=%s "
              "udpPort=%s udpData=%s raw=(Y=%.2f P=%.2f R=%.2f) fov=%.1f yawMode=%s "
              "ads=%s/%s",
        static_cast<unsigned long long>(fields.calls),
        static_cast<unsigned long long>(fields.injected),
        fields.trackingEnabled ? "ON" : "OFF",
        game_state::PhaseName(fields.phase),
        fields.drawing ? "YES" : "NO",
        UdpPortState(fields.receiver), data ? "YES" : "NO", hy, hp, hr,
        g_renderFov.load(std::memory_order_relaxed),
        fields.worldSpaceYaw ? "world" : "local",
        fields.aiming ? "aiming" : "hip",
        AdsModeValue(fields.adsMode));
}

void LogPoseDetail(const ue::FRotator& clean, const frame_pose::FramePose& pose,
                   const ue4::FRotator3f& result, const ue::FVector& positionOffset) {
    static std::atomic<std::uint64_t> s_lastLine{0};
    static std::atomic<int> s_linesWritten{0};
    if (s_linesWritten.load(std::memory_order_relaxed) >= kPoseDetailLines) return;
    const std::uint64_t now = GetTickCount64();
    if ((now - s_lastLine.load(std::memory_order_relaxed)) < kPoseDetailMs) return;
    s_lastLine.store(now, std::memory_order_relaxed);
    s_linesWritten.fetch_add(1, std::memory_order_relaxed);

    Log::Line("pose frame=%llu clean=(Y=%.2f P=%.2f R=%.2f) tracker=(Y=%.2f P=%.2f R=%.2f) "
              "result=(Y=%.2f P=%.2f R=%.2f) headOff_m=(x%.3f y%.3f z%.3f) posOff_ue=(%.1f,%.1f,%.1f)",
        static_cast<unsigned long long>(pose.Frame),
        clean.Yaw, clean.Pitch, clean.Roll,
        pose.Yaw, pose.Pitch, pose.Roll,
        result.Yaw, result.Pitch, result.Roll,
        pose.OffX, pose.OffY, pose.OffZ,
        positionOffset.X, positionOffset.Y, positionOffset.Z);
}

}  // namespace hol_ht::view_diag
