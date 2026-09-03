// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/tracking/head_tracking_session.h"

namespace hol_ht {

// The one tracking session the mod runs: core's session over the OpenTrack UDP
// receiver. Named once here so the hook, the hotkeys and the bootstrap all
// speak about the same type instead of each re-spelling the template.
using Session = cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver>;

}  // namespace hol_ht
