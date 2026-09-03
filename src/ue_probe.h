// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cameraunlock/unreal/ue_runtime.h"

// Two guarded reads off a live UObject that both gates in this mod need: is
// this the class the profile's offsets were taken from, and is the byte at one
// of those offsets actually a bool.
//
// The gameplay gate reads its flags off the APlayerController the view hook
// holds; the ADS gate reads its flag off the ACharacter that controller drives.
// The reads are the same shape, and the invariant they protect is the same:
// an offset that is not where the profile says it is must be caught rather than
// acted on, because a byte of something else toggles several times a second and
// nothing in the log would say why the view keeps stopping.
namespace hol_ht::ue_probe {

// True if `object`'s primary vtable is one of `rvas`. Trailing zero entries are
// unused slots and never match. A vtable outside the game module is not one of
// the classes the profile describes.
template <std::size_t N>
bool VtableIsOneOf(std::uintptr_t object, const std::array<std::uintptr_t, N>& rvas) {
    namespace ue = ::cameraunlock::unreal;
    std::uintptr_t vtable = 0;
    if (!ue::SafeReadPtr(object, vtable) || vtable == 0) return false;
    const std::uintptr_t base = ue::ModuleBase();
    if (vtable < base || vtable >= ue::ModuleEnd()) return false;
    const std::uintptr_t rva = vtable - base;
    for (const std::uintptr_t known : rvas) {
        if (known != 0 && rva == known) return true;
    }
    return false;
}

enum class BoolRead {
    False,
    True,
    Unreadable,  // the address faulted
    NotABool,    // the byte is neither 0 nor 1, so the offset is wrong
};

// Read a one-byte engine bool. Core has no byte-wide guarded read, so this
// takes two and discards the second, which belongs to the next property.
// `byte` receives what was actually there, which is what a NotABool log line
// has to name.
inline BoolRead ReadEngineBool(std::uintptr_t address, unsigned& byte) {
    std::uint16_t raw = 0;
    if (!::cameraunlock::unreal::SafeReadU16(address, raw)) return BoolRead::Unreadable;
    byte = raw & 0xffu;
    switch (byte) {
        case 0: return BoolRead::False;
        case 1: return BoolRead::True;
        default: return BoolRead::NotABool;
    }
}

}  // namespace hol_ht::ue_probe
