// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

#include "cameraunlock/unreal/ue_runtime.h"

// An address in the running process, as an RVA the build profile can be
// compared against. Every hook in this mod tells its callers apart by return
// address, and the build profile records those as RVAs, so this is the
// conversion all of them go through.
namespace hol_ht {

// The RVA of an address in the game module. A module base that has not been
// published yet leaves the address alone, which no profile entry can match.
inline std::uintptr_t ModuleRva(const void* address) {
    const auto addr = reinterpret_cast<std::uintptr_t>(address);
    const auto base = ::cameraunlock::unreal::ModuleBase();
    return base != 0 ? addr - base : addr;
}

// The same conversion, but zero for anything outside the game module. Stack
// walks pick up frames from this DLL, ntdll and drivers; those have no RVA that
// means anything against the profile, and subtracting the base from one would
// produce a huge number that could still collide with a listed RVA by chance.
inline std::uintptr_t ModuleRvaInRange(const void* address) {
    const auto addr = reinterpret_cast<std::uintptr_t>(address);
    const auto base = ::cameraunlock::unreal::ModuleBase();
    if (base == 0 || addr < base || addr >= ::cameraunlock::unreal::ModuleEnd()) return 0;
    return addr - base;
}

}  // namespace hol_ht
