// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <windows.h>

namespace hol_ht {

// Entry point called from DllMain. Spins up a bootstrap thread so the heavy
// work (config, fingerprinting, UDP, MinHook) never runs under the loader lock,
// and pins the module so the hooks it installs can never be unmapped under a
// game thread standing in one of them.
void Initialize(HMODULE self);

}  // namespace hol_ht
