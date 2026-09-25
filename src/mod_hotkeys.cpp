// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "mod_hotkeys.h"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "logging.h"
#include "view_hook.h"

#include "cameraunlock/input/deferred_actions.h"
#include "cameraunlock/input/hotkey_poller.h"
#include "cameraunlock/input/key_binding_registration.h"
#include "cameraunlock/input/key_bindings.h"

namespace hol_ht::hotkeys {

namespace {

using cameraunlock::TrackingMode;
using cameraunlock::input::DeferredAction;
using cameraunlock::input::KeyBinding;

// How often the poller samples the keyboard, in milliseconds.
constexpr unsigned kPollIntervalMs = 16;

std::unique_ptr<cameraunlock::input::HotkeyPoller> g_poller;

// The cycle key computes the next mode from the one the game thread last applied
// and stores it here, so two presses inside one frame land on one step, and the
// game thread applies it at its next frame. See ApplyPending.
std::atomic<TrackingMode> g_appliedMode{TrackingMode::RotationAndPosition};
std::atomic<TrackingMode> g_desiredMode{TrackingMode::RotationAndPosition};
DeferredAction g_applyMode;

const char* ModeName(TrackingMode mode) {
    return mode == TrackingMode::RotationOnly ? "rotation only"
         : mode == TrackingMode::PositionOnly ? "position only"
                                              : "rotation and position";
}

// HeadTrackingSession::CycleMode's order: full, rotation only, position only.
TrackingMode NextMode(TrackingMode mode) {
    return static_cast<TrackingMode>((static_cast<int>(mode) + 1) % 3);
}

// End changes this session only; EnableOnStartup decides the next one.
void ToggleTracking() {
    const bool enabled = !view_hook::TrackingEnabled();
    view_hook::SetTrackingEnabled(enabled);
    Log::Line("hotkey: tracking %s", enabled ? "ON" : "OFF");
}

void ToggleYawMode() {
    const bool worldSpaceYaw = !view_hook::WorldSpaceYaw();
    view_hook::SetWorldSpaceYaw(worldSpaceYaw);
    Log::Line("hotkey: yaw mode %s", worldSpaceYaw ? "world" : "local");
    config::SaveWorldSpaceYaw(worldSpaceYaw);
}

void CycleTrackingMode() {
    const TrackingMode next = NextMode(g_appliedMode.load());
    g_desiredMode.store(next);
    g_applyMode.Request();
    config::SaveTrackingMode(next);
}

// The table's hotkey codec only lets through a list this parser reads.
std::vector<KeyBinding> Bindings(const char* key, const std::string& list) {
    const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(list);
    if (!parsed.ok()) throw std::logic_error(std::string(key) + "='" + list + "': " + parsed.error);
    return parsed.bindings;
}

}  // namespace

void Register(const Config& config, TrackingMode applied) {
    g_appliedMode.store(applied);
    g_poller = std::make_unique<cameraunlock::input::HotkeyPoller>();
    cameraunlock::input::RegisterKeyBindings(*g_poller, Bindings("ToggleKey", config.toggle_key),
                                             [] { ToggleTracking(); });
    cameraunlock::input::RegisterKeyBindings(
        *g_poller, Bindings("CycleTrackingModeKey", config.cycle_tracking_mode_key), [] { CycleTrackingMode(); });
    cameraunlock::input::RegisterKeyBindings(*g_poller, Bindings("YawModeKey", config.yaw_mode_key),
                                             [] { ToggleYawMode(); });
    g_poller->Start(kPollIntervalMs);
}

bool ApplyPending(Session& session) {
    if (!g_applyMode.Consume()) return false;
    const TrackingMode mode = g_desiredMode.load();
    session.SetMode(mode);
    g_appliedMode.store(mode);
    Log::Line("hotkey: tracking mode -> %s", ModeName(mode));
    return true;
}

}  // namespace hol_ht::hotkeys
