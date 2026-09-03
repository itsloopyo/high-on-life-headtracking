// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "ads_state.h"

#include <atomic>

#include "builds/build_registry.h"
#include "logging.h"
#include "ue_probe.h"

#include "cameraunlock/unreal/ue_runtime.h"

namespace hol_ht::ads_state {

namespace {

namespace ue = ::cameraunlock::unreal;

// Cleared for the session if ADSOn ever reads as something that is not a bool,
// which means it is not where the build profile says it is. Acting on such a
// byte would put the sights up and down at random, several times a second, with
// nothing in the log saying why the view keeps stopping.
std::atomic<bool> g_offsetTrusted{true};

}  // namespace

bool IsAimingDownSights(std::uintptr_t controller) {
    if (!g_offsetTrusted.load(std::memory_order_relaxed)) return false;

    std::uintptr_t character = 0;
    if (!ue::SafeReadPtr(controller + Offsets().kCharacterOffset, character) || character == 0)
        return false;
    if (!ue_probe::VtableIsOneOf(character, Offsets().kPlayerCharacterVtableRvas)) return false;

    unsigned byte = 0;
    switch (ue_probe::ReadEngineBool(character + Offsets().kAdsOnOffset, byte)) {
        case ue_probe::BoolRead::True:       return true;
        case ue_probe::BoolRead::False:      return false;
        case ue_probe::BoolRead::Unreadable: return false;
        case ue_probe::BoolRead::NotABool:   break;
    }

    if (g_offsetTrusted.exchange(false, std::memory_order_relaxed)) {
        Log::Line("ads: character+0x%zx holds %u, which is not a bool - ADSOn is not where "
                  "this build profile says it is. Head tracking will behave as though the "
                  "sights are never raised for this session.",
                  Offsets().kAdsOnOffset, byte);
    }
    return false;
}

}  // namespace hol_ht::ads_state
