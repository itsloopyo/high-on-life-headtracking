// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cameraunlock/logging/file_log.h>

// The process-wide log lives in cameraunlock-core (logging::Open/Close/Line).
// Alias it under the mod namespace so call sites read Log::Line(...).
namespace hol_ht
{
    namespace Log = ::cameraunlock::logging;
}  // namespace hol_ht
