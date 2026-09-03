// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "presentation_gate.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <set>

#include <windows.h>

#include "builds/build_profile.h"
#include "logging.h"
#include "module_rva.h"

namespace hol_ht::presentation {

namespace {

// Frames captured per query. The profile says how many of them to look at;
// this is the ceiling the buffer is sized for.
constexpr ULONG kMaxFrames = 16;

std::uintptr_t g_allowed[kPresentationCallerSlots]{};
std::size_t    g_allowedCount = 0;
ULONG          g_frames = 0;

// Bounded: the point is to name the consumers that were missed, and there are
// only ever a handful of distinct ones. Left unbounded it would write a line
// per frame forever for whatever gameplay caller runs every tick.
//
// The count is read once outside the lock so that, after the last line is
// written, a gameplay caller running every tick stops paying for a mutex on
// every view query for the rest of the session. The count inside the lock is
// still the one that decides.
constexpr int kMaxUnmatchedLines = 12;
std::mutex g_unmatchedMutex;
std::set<std::uintptr_t> g_unmatchedSeen;
std::atomic<int> g_unmatchedLines{0};

// The stack from the last CallerIsDrawing() call on this thread, so
// LogUnmatched() reports the stack that was actually rejected rather than
// walking it a second time and getting a different one.
thread_local void*  t_frames[kMaxFrames]{};
thread_local ULONG  t_frameCount = 0;

}  // namespace

void Init(const std::uintptr_t* rvas, std::size_t rvaCount, std::uint32_t stackFrames) {
    g_allowedCount = 0;
    for (std::size_t i = 0; i < rvaCount && g_allowedCount < kPresentationCallerSlots; ++i) {
        if (rvas[i] != 0) g_allowed[g_allowedCount++] = rvas[i];
    }
    g_frames = stackFrames;
    if (g_frames > kMaxFrames) g_frames = kMaxFrames;
    Log::Line("presentation-gate: %zu drawing consumers allow-listed, %lu stack frames scanned",
        g_allowedCount, g_frames);
}

std::uintptr_t MatchDrawingCaller(void* const* frames, unsigned long count) {
    for (unsigned long i = 0; i < count; ++i) {
        const std::uintptr_t rva = ModuleRvaInRange(frames[i]);
        if (rva == 0) continue;
        for (std::size_t k = 0; k < g_allowedCount; ++k) {
            if (rva == g_allowed[k]) return rva;
        }
    }
    return 0;
}

bool CallerIsDrawing() {
    // Frame 0 is this function's own caller (the hook), which is never in the
    // game module, so nothing is lost by starting the capture at 0 and letting
    // Rva() reject it.
    t_frameCount = RtlCaptureStackBackTrace(0, g_frames, t_frames, nullptr);
    return MatchDrawingCaller(t_frames, t_frameCount) != 0;
}

void LogUnmatched() {
    if (g_unmatchedLines.load(std::memory_order_relaxed) >= kMaxUnmatchedLines) return;

    // Key on the deepest in-module frame, which is the consumer: keying on the
    // whole stack would log the same caller once per distinct path into it.
    std::uintptr_t deepest = 0;
    for (ULONG i = 0; i < t_frameCount; ++i) {
        const std::uintptr_t rva = ModuleRvaInRange(t_frames[i]);
        if (rva != 0) deepest = rva;
    }
    if (deepest == 0) return;

    std::lock_guard<std::mutex> lk(g_unmatchedMutex);
    if (g_unmatchedLines.load(std::memory_order_relaxed) >= kMaxUnmatchedLines) return;
    if (!g_unmatchedSeen.insert(deepest).second) return;
    g_unmatchedLines.fetch_add(1, std::memory_order_relaxed);

    char buf[512];
    int n = std::snprintf(buf, sizeof(buf), "presentation-gate: not drawing, view left clean; frames");
    for (ULONG i = 0; i < t_frameCount && n > 0 && n < static_cast<int>(sizeof(buf)); ++i) {
        const std::uintptr_t rva = ModuleRvaInRange(t_frames[i]);
        if (rva == 0) continue;
        n += std::snprintf(buf + n, sizeof(buf) - n, " 0x%08llx",
                           static_cast<unsigned long long>(rva));
    }
    Log::Line("%s", buf);
}

}  // namespace hol_ht::presentation
