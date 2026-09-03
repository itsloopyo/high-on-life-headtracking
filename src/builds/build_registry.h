// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once
#include <windows.h>
#include "build_profile.h"

// Profile registry and selection. SelectProfile() fingerprints the host EXE
// (PE TimeDateStamp + SizeOfImage + CheckSum) and installs the matching profile
// as active, or stays dormant if no profile claims this build.

namespace hol_ht::builds {

enum class MatchResult {
    Matched,     // Active profile set; mod can run.
    ReadFailed,  // Could not read the PE header.
    HostNewer,   // Running EXE TimeDateStamp > primary profile.
    HostOlder,   // Running EXE TimeDateStamp < primary profile.
    HostDiffers, // Same timestamp, different size or checksum.
    // The fingerprint matched a profile this mod cannot use: an RVA outside the
    // image. That is a defect in the profile, not in the player's install, and
    // it must never be reported as a modified binary.
    ProfileUnusable,
};

MatchResult SelectProfile(HMODULE host);
const BuildProfile& ActiveProfile();

}  // namespace hol_ht::builds

namespace hol_ht {

// Accessor for the active profile's offset table. Must run after
// SelectProfile() returns Matched.
inline const OffsetTable& Offsets() {
    return builds::ActiveProfile().Offsets;
}

}  // namespace hol_ht
