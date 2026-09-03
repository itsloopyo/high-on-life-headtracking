// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "mod_hotkeys.h"

#include <memory>

#include "ads.h"
#include "logging.h"
#include "view_hook.h"

#include "cameraunlock/input/chord_hotkeys.h"
#include "cameraunlock/input/deferred_actions.h"
#include "cameraunlock/input/hotkey_poller.h"

namespace hol_ht::hotkeys {

namespace {

using cameraunlock::TrackingMode;
using cameraunlock::input::ChordGuarded;
using cameraunlock::input::DeferredAction;
using cameraunlock::input::NavGuarded;

// Virtual-key codes. The nav-cluster defaults and the Ctrl+Shift chord cluster
// (T/Y/U/G/H/J) are the fleet-wide bindings from AGENTS.md, so the same action
// sits on the same key in every mod.
constexpr int kVkEnd    = 0x23;
constexpr int kVkPageUp = 0x21;
constexpr int kVkY      = 0x59;
constexpr int kVkG      = 0x47;
constexpr int kVkH      = 0x48;
constexpr int kVkU      = 0x55;

// How often the poller samples the keyboard, in milliseconds.
constexpr unsigned kPollIntervalMs = 16;

std::unique_ptr<cameraunlock::input::HotkeyPoller> g_poller;

// The one binding whose work belongs to the game thread. See ApplyPending.
DeferredAction g_cycleTrackingMode;

void ToggleTracking() {
    const bool enabled = !view_hook::TrackingEnabled();
    view_hook::SetTrackingEnabled(enabled);
    Log::Line("hotkey: tracking %s", enabled ? "ON" : "OFF");
}

void ToggleYawMode() {
    const bool worldSpaceYaw = !view_hook::WorldSpaceYaw();
    view_hook::SetWorldSpaceYaw(worldSpaceYaw);
    Log::Line("hotkey: yaw mode %s", worldSpaceYaw ? "world" : "local");
}

// The mode the hook reads once per frame, so the change lands on the aim that is
// already in progress rather than on the next one. Saved as it is cycled,
// because the choice is the player's and a firefight is a bad place to lose it.
void CycleAdsMode() {
    const AdsMode next = NextAdsModeTwoSlot(view_hook::GetAdsMode());
    view_hook::SetAdsMode(next);
    config::SaveAdsMode(next);
    Log::Line("hotkey: %s", AdsModeToast(next));
}

}  // namespace

void Register(const Config& config) {
    g_poller = std::make_unique<cameraunlock::input::HotkeyPoller>();

    // Nav-cluster defaults. Suppressed when Ctrl+Shift is held so the chord
    // path is the sole trigger.
    g_poller->AddHotkey(kVkEnd,    NavGuarded([] { ToggleTracking(); }));
    g_poller->AddHotkey(kVkPageUp, NavGuarded([] { g_cycleTrackingMode.Request(); }));
    g_poller->AddHotkey(config.yaw_mode_key, NavGuarded([] { ToggleYawMode(); }));
    g_poller->AddHotkey(config.ads_mode_key, NavGuarded([] { CycleAdsMode(); }));

    // Ctrl+Shift chord alternatives (Y/G/H/U cluster).
    g_poller->AddHotkey(kVkY, ChordGuarded([] { ToggleTracking(); }));
    g_poller->AddHotkey(kVkG, ChordGuarded([] { g_cycleTrackingMode.Request(); }));
    g_poller->AddHotkey(kVkH, ChordGuarded([] { ToggleYawMode(); }));
    g_poller->AddHotkey(kVkU, ChordGuarded([] { CycleAdsMode(); }));

    g_poller->Start(kPollIntervalMs);
}

bool ApplyPending(Session& session) {
    if (!g_cycleTrackingMode.Consume()) return false;
    const TrackingMode mode = session.CycleMode();
    const char* name = mode == TrackingMode::RotationOnly ? "rotation only"
                     : mode == TrackingMode::PositionOnly ? "position only"
                                                          : "rotation and position";
    Log::Line("hotkey: tracking mode -> %s", name);
    return true;
}

}  // namespace hol_ht::hotkeys
