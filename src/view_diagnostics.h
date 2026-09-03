// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

#include "ads.h"
#include "frame_pose.h"
#include "game_state.h"
#include "ue_types.h"

#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/unreal/ue_math.h"

// What the view hook writes to the log, and nothing it does to the frame.
//
// Every function here is bounded - a heartbeat on a timer, a fixed number of
// pose lines - because the alternative on a hook that fires several times a
// frame is a log file that fills a disk. Keeping them out of view_hook.cpp
// leaves that file reading as what happens to the camera, which is the question
// it exists to answer.
namespace hol_ht::view_diag {

// The FOV the frame is being drawn with, read from the FMinimalViewInfo the
// consumer is filling in. Diagnostics only - the game has its own FOV setting,
// so the mod has no business changing it - but a user reporting that the
// crosshair sits wrong needs the log to say what the frame was drawn at, and
// the stride test is the cheapest possible proof that the caller gate really
// did land on an FMinimalViewInfo builder.
void ReadRenderFov(const ue4::FVector3f* outLocation, const ue4::FRotator3f* outRotation);

struct HeartbeatFields {
    std::uint64_t calls;
    std::uint64_t injected;
    bool trackingEnabled;
    game_state::Phase phase;
    bool drawing;
    const cameraunlock::UdpReceiver* receiver;
    bool worldSpaceYaw;
    bool aiming;
    AdsMode adsMode;
};

// True on the hook's first call and at most once per kHeartbeatMs after it,
// claiming the slot as it answers. Split from the line below so a caller only
// pays for the fields it is about to log: classifying the player controller
// costs four fault-guarded reads, and this line is written twice a minute.
bool HeartbeatDue(std::uint64_t calls);

// The line itself. Call only when HeartbeatDue() said so.
void LogHeartbeat(const HeartbeatFields& fields);

// The clean rotation, the tracker pose and what the two composed to, for the
// first few injected frames.
void LogPoseDetail(const cameraunlock::unreal::FRotator& clean,
                   const frame_pose::FramePose& pose,
                   const ue4::FRotator3f& result,
                   const cameraunlock::unreal::FVector& positionOffset);

}  // namespace hol_ht::view_diag
