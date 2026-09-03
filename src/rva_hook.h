// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

// Creating and enabling one trampoline against a build-profile RVA.
//
// All three of this mod's hooks - the view query, the crosshair projection and
// the aim probe - are the same four steps: resolve the RVA against the module
// base, create the trampoline, enable it, and say in the log which of them
// refused. Writing that out three times is three places for the failure path to
// drift apart.
namespace hol_ht {

// Returns false, having logged why under `channel`, if MinHook refuses. `name`
// is the hooked function, so the log names it rather than the RVA alone.
bool InstallRvaHook(const char* channel, const char* name, std::uintptr_t rva,
                    void* detour, void** original);

}  // namespace hol_ht
