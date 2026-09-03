// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "game_state.h"

#include <atomic>
#include <cstddef>

#include "builds/build_registry.h"
#include "logging.h"
#include "ue_probe.h"

#include "cameraunlock/unreal/ue_runtime.h"

namespace hol_ht::game_state {

namespace {

namespace ue = ::cameraunlock::unreal;

// Cleared for the session if either game-specific flag ever reads as something
// that is not a bool, which means it is not where the build profile says it is.
// Acting on such a byte would be a coin toss between "paused" and "not" on
// every frame, i.e. head tracking dying at random with nothing in the log.
std::atomic<bool> g_flagsTrusted{true};

// Returns false when the byte is not a bool, having disabled both flags for the
// session.
bool ReadFlag(std::uintptr_t controller, std::size_t offset, bool& out) {
    if (!g_flagsTrusted.load(std::memory_order_relaxed)) return false;

    unsigned byte = 0;
    switch (ue_probe::ReadEngineBool(controller + offset, byte)) {
        case ue_probe::BoolRead::False:
            out = false;
            return true;
        case ue_probe::BoolRead::True:
            out = true;
            return true;
        case ue_probe::BoolRead::Unreadable:
            return false;
        case ue_probe::BoolRead::NotABool:
            break;
    }

    if (g_flagsTrusted.exchange(false, std::memory_order_relaxed)) {
        Log::Line("game-state: controller+0x%zx holds %u, which is not a bool - the "
                  "gameplay flags are not where this build profile says they are. "
                  "Falling back to the mouse-cursor gate alone for this session.",
                  offset, byte);
    }
    return false;
}

bool MouseCursorRaised(std::uintptr_t controller) {
    std::uint32_t flags = 0;
    if (!ue::SafeReadU32(controller + Offsets().kShowMouseCursorOffset, flags))
        return true;  // unreadable controller: treat as not gameplay
    return (flags & Offsets().kShowMouseCursorMask) != 0;
}

}  // namespace

Phase Classify(std::uintptr_t controller) {
    if (!ue_probe::VtableIsOneOf(controller, Offsets().kGameplayControllerVtableRvas))
        return Phase::UnknownClass;

    bool paused = false;
    if (ReadFlag(controller, Offsets().kPauseMenuActiveOffset, paused) && paused)
        return Phase::Menu;

    bool cinematic = false;
    if (ReadFlag(controller, Offsets().kCinematicModeActiveOffset, cinematic) && cinematic)
        return Phase::Cinematic;

    if (MouseCursorRaised(controller)) return Phase::Menu;

    return Phase::Gameplay;
}

const char* PhaseName(Phase p) {
    switch (p) {
        case Phase::Gameplay:     return "gameplay";
        case Phase::Menu:         return "menu/paused";
        case Phase::Cinematic:    return "cinematic";
        case Phase::UnknownClass: return "non-gameplay controller";
    }
    return "?";
}

void LogTransition(Phase phase) {
    static std::atomic<int> s_last{-1};
    const int now = static_cast<int>(phase);
    if (s_last.exchange(now, std::memory_order_relaxed) == now) return;
    Log::Line("game-state: %s - head tracking %s",
              PhaseName(phase), IsGameplay(phase) ? "ON" : "suppressed");
}

}  // namespace hol_ht::game_state
