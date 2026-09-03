// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "build_registry.h"

#include <array>

#include <cameraunlock/memory/pe_fingerprint.h>

#include "logging.h"

namespace hol_ht::builds {

// One extern per known build. Never-delete policy: when a game patch breaks
// the current build, derive new RVAs and ADD a new profile here (newest at
// the top of kKnownProfiles) without removing the old one. Users on the
// un-patched build still match their old profile by PE fingerprint.
extern const BuildProfile kSteamProfile_20231025;

namespace {

// Newest-first. The first entry is the "primary" used to label newer/older when
// no profile matches.
constexpr std::array<const BuildProfile*, 1> kKnownProfiles = {
    &kSteamProfile_20231025,
};

const BuildProfile* g_active = nullptr;

// Every RVA in a profile must land inside the image the fingerprint pins, and
// the hook target must be present at all. The fingerprint already guarantees we
// are looking at the exact EXE the numbers were read from, so a violation here
// is a typo in the profile rather than anything about the player's install -
// and the failure it prevents is the expensive one: hooking a garbage address
// crashes the game seconds in, which AGENTS.md makes the failsafe mandatory for.
//
// Struct field offsets are deliberately not checked. They are offsets into a
// UObject, not into the module, and have nothing to do with SizeOfImage.
bool ProfileIsUsable(const BuildProfile* p, const char*& why) {
    const OffsetTable& o = p->Offsets;
    if (o.kGetPlayerViewPointRva == 0) {
        why = "no GetPlayerViewPoint RVA";
        return false;
    }

    const std::uintptr_t limit = p->Fingerprint.SizeOfImage;
    auto inImage = [limit](std::uintptr_t rva) { return rva == 0 || rva < limit; };

    if (!inImage(o.kGetPlayerViewPointRva) || !inImage(o.kViewPointCallerRva) ||
        !inImage(o.kGFrameCounterRva) ||
        !inImage(o.kGetAimLocationRva) || !inImage(o.kCrosshairAimCallerRva)) {
        why = "an RVA lies outside the image";
        return false;
    }
    for (std::uintptr_t rva : o.kPresentationCallerRvas) {
        if (!inImage(rva)) { why = "a presentation caller RVA lies outside the image"; return false; }
    }
    for (std::uintptr_t rva : o.kGameplayControllerVtableRvas) {
        if (!inImage(rva)) { why = "a controller vtable RVA lies outside the image"; return false; }
    }
    for (std::uintptr_t rva : o.kPlayerCharacterVtableRvas) {
        if (!inImage(rva)) { why = "a character vtable RVA lies outside the image"; return false; }
    }
    return true;
}

}  // namespace

MatchResult SelectProfile(HMODULE host) {
    PeFingerprint running{};
    if (!cameraunlock::memory::ReadPeFingerprint(host, running)) {
        Log::Line("build-check: failed to read PE header from host module");
        return MatchResult::ReadFailed;
    }

    Log::Line("build-check: running  ts=0x%08x size=0x%08x csum=0x%08x",
        running.TimeDateStamp, running.SizeOfImage, running.CheckSum);

    for (const BuildProfile* p : kKnownProfiles) {
        const char* why = nullptr;
        const bool usable = ProfileIsUsable(p, why);
        Log::Line("build-check: profile=%s ts=0x%08x size=0x%08x csum=0x%08x%s%s",
            p->Name, p->Fingerprint.TimeDateStamp,
            p->Fingerprint.SizeOfImage, p->Fingerprint.CheckSum,
            usable ? "" : " (unusable: ", usable ? "" : why);
        if (running.Matches(p->Fingerprint)) {
            if (!usable) {
                Log::Line("build-check: your game matches profile %s, but that profile is "
                          "faulty (%s). This is a bug in the mod, not in your install - "
                          "please report it. Staying dormant; game runs vanilla.",
                          p->Name, why);
                return MatchResult::ProfileUnusable;
            }
            g_active = p;
            Log::Line("build-check: matched profile %s", p->Name);
            return MatchResult::Matched;
        }
    }

    // No match. Classify against the primary profile so the log explains
    // direction ("patched newer", "older", or "tampered").
    switch (cameraunlock::memory::ClassifyMismatch(
                running, kKnownProfiles.front()->Fingerprint)) {
        case cameraunlock::memory::FingerprintMismatch::Newer:
            return MatchResult::HostNewer;
        case cameraunlock::memory::FingerprintMismatch::Older:
            return MatchResult::HostOlder;
        case cameraunlock::memory::FingerprintMismatch::Differs:
        default:
            return MatchResult::HostDiffers;
    }
}

const BuildProfile& ActiveProfile() { return *g_active; }

}  // namespace hol_ht::builds
