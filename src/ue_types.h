// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cameraunlock/unreal/ue_math.h>

// High On Life is Unreal Engine 4, so its FVector and FRotator are three
// FLOATS - 12 bytes each - not the three doubles UE 5.0+ ships with Large
// World Coordinates on. Core's cameraunlock::unreal::FVector / FRotator are
// the 24-byte LWC types, and passing one of those to a function that writes a
// 12-byte out-param overruns the caller's stack frame by twelve bytes and
// decodes adjacent bytes as coordinates.
//
// So the ENGINE ABI types live here, and everything above the hook keeps using
// core's double types for the maths. The two conversions are the only place
// the float/double boundary exists.
namespace hol_ht::ue4 {

struct FVector3f  { float X, Y, Z; };
struct FRotator3f { float Pitch, Yaw, Roll; };
struct FVector2Df { float X, Y; };

static_assert(sizeof(FVector3f) == 12, "UE4 FVector is three floats");
static_assert(sizeof(FRotator3f) == 12, "UE4 FRotator is three floats");
static_assert(sizeof(FVector2Df) == 8, "UE4 FVector2D is two floats");

inline ::cameraunlock::unreal::FRotator ToCore(const FRotator3f& r) {
    return { static_cast<double>(r.Pitch),
             static_cast<double>(r.Yaw),
             static_cast<double>(r.Roll) };
}

inline void FromCore(const ::cameraunlock::unreal::FRotator& src, FRotator3f& dst) {
    dst.Pitch = static_cast<float>(src.Pitch);
    dst.Yaw   = static_cast<float>(src.Yaw);
    dst.Roll  = static_cast<float>(src.Roll);
}

}  // namespace hol_ht::ue4
