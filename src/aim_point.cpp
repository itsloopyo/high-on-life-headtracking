// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "aim_point.h"

#include <atomic>
#include <cmath>
#include <cstdint>

#include <windows.h>
#include <intrin.h>

#include "builds/build_registry.h"
#include "logging.h"
#include "module_rva.h"
#include "rva_hook.h"
#include "ue_types.h"
#include "view_hook.h"

namespace hol_ht::aim_point {

namespace {

using ue4::FVector3f;

// USQFiringResultComponent::GetAimLocation(self, &OutAimLocation,
// bUseAimCorrections), which returns the out-pointer it was handed.
using GetAimLocation_t = FVector3f*(__fastcall*)(void* self, FVector3f* out, bool corrections);

GetAimLocation_t g_orig = nullptr;
bool g_logDistance = false;

constexpr std::uint64_t kLogIntervalMs = 2000;
constexpr int kMaxLines = 30;

void LogDistance(const FVector3f& aim) {
    static std::atomic<std::uint64_t> s_lastLine{0};
    static std::atomic<int> s_lines{0};
    if (s_lines.load(std::memory_order_relaxed) >= kMaxLines) return;
    const std::uint64_t now = GetTickCount64();
    if ((now - s_lastLine.load(std::memory_order_relaxed)) < kLogIntervalMs) return;

    // Before the budget is spent, not after: the crosshair can ask for an aim
    // point before the first drawing view query of the session, and a
    // measurement with no camera to measure from must not consume one of the
    // thirty lines.
    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    if (!view_hook::CleanCameraLocation(cx, cy, cz)) return;

    s_lastLine.store(now, std::memory_order_relaxed);
    s_lines.fetch_add(1, std::memory_order_relaxed);

    const double dx = static_cast<double>(aim.X) - cx;
    const double dy = static_cast<double>(aim.Y) - cy;
    const double dz = static_cast<double>(aim.Z) - cz;
    const double metres = std::sqrt(dx * dx + dy * dy + dz * dz) / 100.0;

    Log::Line("aim-point: crosshair aim point %.2f m from the camera "
              "(world %.0f,%.0f,%.0f)",
        metres, static_cast<double>(aim.X), static_cast<double>(aim.Y),
        static_cast<double>(aim.Z));
}

FVector3f* __fastcall GetAimLocation_Hook(void* self, FVector3f* out, bool corrections) {
    const std::uintptr_t retRva = ModuleRva(_ReturnAddress());
    if (retRva != Offsets().kCrosshairAimCallerRva)
        return g_orig(self, out, corrections);

    // The one substitution this mod makes to the game's own aim: the crosshair
    // asks without corrections and gets a ray end, so ask again with them and
    // get the traced impact point. Every other caller - the projectile spawn,
    // the arc solver, the NPC paths - keeps the flag it chose.
    FVector3f* result = g_orig(self, out, true);
    if (g_logDistance && result != nullptr) LogDistance(*result);
    return result;
}

}  // namespace

bool Install(bool logDistance) {
    g_logDistance = logDistance;

    const std::uintptr_t rva = Offsets().kGetAimLocationRva;
    if (rva == 0 || Offsets().kCrosshairAimCallerRva == 0) {
        Log::Line("aim-point: this build profile has no GetAimLocation RVA - the crosshair "
                  "keeps the fixed ray end the game draws it from, which is correct until "
                  "you lean");
        return false;
    }

    if (!InstallRvaHook("aim-point:", "GetAimLocation", rva,
                        reinterpret_cast<void*>(&GetAimLocation_Hook),
                        reinterpret_cast<void**>(&g_orig)))
        return false;

    Log::Line("aim-point: the crosshair's aim point now comes from the game's own aim trace "
              "(RVA 0x%08llx, caller 0x%08llx), so it marks where the shot stops at any range",
        static_cast<unsigned long long>(rva),
        static_cast<unsigned long long>(Offsets().kCrosshairAimCallerRva));
    return true;
}

}  // namespace hol_ht::aim_point
