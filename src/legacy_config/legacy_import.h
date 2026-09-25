// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "config.h"

#include "cameraunlock/config/legacy_import.h"

// The legacy import the config owner runs on a HeadTracking.ini with no
// [CameraUnlock] stamp: the frozen reader in legacy_config.h, then a map from
// what it read into the canonical Config. Frozen like the reader: never edit it,
// since a player can update from any older build.
namespace hol_ht::legacy {

cameraunlock::config::LegacyImport<hol_ht::Config> Import();

}  // namespace hol_ht::legacy
