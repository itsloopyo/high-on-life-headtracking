// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// The INI contract, for the two keys whose wrong answer is silent.
//
// The processor clamps y as [-limit_y_down, +limit_y], and limit_y_down carries
// its own default. An INI written before the LimitYDown key existed carries only
// LimitY, so LimitY has to reach both bounds or the player gets asymmetric travel
// with nothing in the log saying why.
//
// AdsMode names a slot in the fleet-wide cycle, and an unrecognised value has to
// land on the default rather than on whichever branch happens to be last - a
// player who typos it must not end up with head tracking through their sights
// that they did not ask for.

#include <cstdio>
#include <string>

#include <windows.h>

#include "config.h"
#include "test_harness.h"

#include "cameraunlock/data/position_settings.h"

namespace {

// Every value asserted here is a float straight out of the INI reader, so the
// tolerance is the one float representation error, not a computed drift.
constexpr double kFloatTolerance = 1e-6;

#define CHECK_LIMIT(actual, expected, what) \
    CHECK_NEAR_MSG((actual), (expected), kFloatTolerance, (what))

// IniReader reads through GetPrivateProfileString, which caches the file it last
// read, so every case gets a directory of its own under TEMP.
std::string WriteIni(const char* tag, const char* body) {
    char temp[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, temp);
    const std::string dir = std::string(temp) + "hol_ht_config_" + tag;
    CreateDirectoryA(dir.c_str(), nullptr);

    const std::string path = dir + "\\HeadTracking.ini";
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "w");
    if (f == nullptr) {
        std::printf("could not write %s\n", path.c_str());
        CHECK_MSG(false, "the test could not write its own INI");
        return dir;
    }
    std::fputs(body, f);
    std::fclose(f);
    return dir;
}

void LimitYReachesBothBoundsWhenLimitYDownIsAbsent() {
    hol_ht::Config raised;
    hol_ht::config::Load(WriteIni("wide", "[Position]\nLimitY=0.40\n"), raised);
    CHECK_LIMIT(raised.limit_y, 0.40f, "LimitY=0.40 raises the upward bound");
    CHECK_LIMIT(raised.limit_y_down, 0.40f,
                "LimitY=0.40 raises the downward bound too, rather than leaving 0.20");

    hol_ht::Config tightened;
    hol_ht::config::Load(WriteIni("tight", "[Position]\nLimitY=0.05\n"), tightened);
    CHECK_LIMIT(tightened.limit_y, 0.05f, "LimitY=0.05 lowers the upward bound");
    CHECK_LIMIT(tightened.limit_y_down, 0.05f, "LimitY=0.05 lowers the downward bound too");
}

void AnExplicitLimitYDownStillWins() {
    hol_ht::Config cfg;
    hol_ht::config::Load(WriteIni("both", "[Position]\nLimitY=0.40\nLimitYDown=0.05\n"), cfg);
    CHECK_LIMIT(cfg.limit_y, 0.40f, "LimitY is read");
    CHECK_LIMIT(cfg.limit_y_down, 0.05f, "an explicit LimitYDown overrides the mirrored value");
}

void EveryAdsModeValueRoundTripsAndATypoFallsBackToTheDefault() {
    hol_ht::Config tracked;
    hol_ht::config::Load(WriteIni("ads_tracked", "[View]\nAdsMode=tracked\n"), tracked);
    CHECK_MSG(tracked.ads_mode == hol_ht::AdsMode::Tracked, "AdsMode=tracked is read as tracked");

    hol_ht::Config paused;
    hol_ht::config::Load(WriteIni("ads_paused", "[View]\nAdsMode=paused\n"), paused);
    CHECK_MSG(paused.ads_mode == hol_ht::AdsMode::Paused, "AdsMode=paused is read as paused");

    // This mod ships two slots, so `marker` is not one of its values. A file
    // carrying it - written by a three-slot sibling, or by a release of this mod
    // from before the game's own ADS reticle was confirmed - must land on the
    // default rather than switching head tracking on through the sights.
    hol_ht::Config marker;
    hol_ht::config::Load(WriteIni("ads_marker", "[View]\nAdsMode=marker\n"), marker);
    CHECK_MSG(marker.ads_mode == hol_ht::kDefaultAdsMode,
              "AdsMode=marker is not a value this mod offers and falls back to the default");

    hol_ht::Config typo;
    hol_ht::config::Load(WriteIni("ads_typo", "[View]\nAdsMode=trakced\n"), typo);
    CHECK_MSG(typo.ads_mode == hol_ht::kDefaultAdsMode,
              "an unrecognised AdsMode lands on the default rather than the last branch");
}

// A hotkey is the third silent one: a virtual-key code outside 0x01-0xFE binds
// the action to a code no keyboard produces, so the key does nothing in game and
// nothing on screen says why.
void AnImpossibleVirtualKeyFallsBackToTheDefault() {
    hol_ht::Config bad;
    hol_ht::config::Load(WriteIni("vk_bad", "[Hotkeys]\nYawMode=0x999\nAdsMode=0\n"), bad);
    CHECK_MSG(bad.yaw_mode_key == hol_ht::Config{}.yaw_mode_key,
              "a YawMode past 0xFE falls back to the default key");
    CHECK_MSG(bad.ads_mode_key == hol_ht::Config{}.ads_mode_key,
              "an AdsMode of 0x00 falls back to the default key");

    hol_ht::Config good;
    hol_ht::config::Load(WriteIni("vk_good", "[Hotkeys]\nYawMode=0x2E\n"), good);
    CHECK_MSG(good.yaw_mode_key == 0x2E, "a valid virtual-key code is kept");
}

// A float key is the fourth silent one, and the worst of the four. strtod
// accepts "nan" and "inf", and a range clamp written as two comparisons lets a
// NaN through both of them - so `Yaw=nan` reaches the sensitivity, the composed
// FRotator, and the frame the player is looking at. Every float key has to reject
// a non-finite value at the boundary and keep its default.
void ANonFiniteFloatFallsBackToTheDefault() {
    hol_ht::Config nonFinite;
    hol_ht::config::Load(
        WriteIni("float_nan", "[Sensitivity]\nYaw=nan\nPitch=NAN\n[Smoothing]\nLocal=nan\n"), nonFinite);
    CHECK_MSG(nonFinite.yaw_sensitivity == hol_ht::Config{}.yaw_sensitivity,
              "Yaw=nan keeps the default sensitivity rather than reaching the camera");
    CHECK_MSG(nonFinite.pitch_sensitivity == hol_ht::Config{}.pitch_sensitivity,
              "Pitch=NAN keeps the default sensitivity");
    CHECK_MSG(nonFinite.local_smoothing == hol_ht::Config{}.local_smoothing,
              "Local=nan keeps the default smoothing");

    hol_ht::Config infinite;
    hol_ht::config::Load(
        WriteIni("float_inf", "[Position]\nLimitZ=inf\nSensitivityX=-inf\n"), infinite);
    CHECK_MSG(infinite.limit_z == hol_ht::Config{}.limit_z,
              "LimitZ=inf keeps the default rather than clamping to the maximum travel");
    CHECK_MSG(infinite.position_sensitivity_x == hol_ht::Config{}.position_sensitivity_x,
              "SensitivityX=-inf keeps the default");

    // A finite out-of-range value still clamps, as it always has.
    hol_ht::Config wide;
    hol_ht::config::Load(WriteIni("float_wide", "[Sensitivity]\nYaw=99\n"), wide);
    CHECK_LIMIT(wide.yaw_sensitivity, 3.0f, "a finite Yaw above the range still clamps to 3.0");
}

// WriteDefaultIfMissing must never touch an INI that is already there. It is
// called on every launch, so an overwrite here silently resets every key the
// player tuned - and the file it would clobber is the only place their settings
// live.
void WriteDefaultLeavesAnExistingIniAlone() {
    const std::string dir = WriteIni("existing", "[Sensitivity]\nYaw=2.50\n");

    hol_ht::config::WriteDefaultIfMissing(dir);

    hol_ht::Config cfg;
    hol_ht::config::Load(dir, cfg);
    CHECK_LIMIT(cfg.yaw_sensitivity, 2.50f,
                "an INI that already exists keeps the value the player put in it");
}

// The generated default INI must state core's actual limit_y_down default, and
// the AdsMode slot the mod actually starts in - not literals that can drift from
// them silently.
void WrittenDefaultIniMatchesTheCodeItDocuments() {
    char temp[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, temp);
    const std::string dir = std::string(temp) + "hol_ht_config_written_default";
    CreateDirectoryA(dir.c_str(), nullptr);
    // config::WriteDefaultIfMissing is a no-op once the file exists, so a leftover
    // from a previous run of this test would hide a regression here.
    DeleteFileA((dir + "\\HeadTracking.ini").c_str());

    hol_ht::config::WriteDefaultIfMissing(dir);

    hol_ht::Config cfg;
    hol_ht::config::Load(dir, cfg);
    CHECK_LIMIT(cfg.limit_y_down, cameraunlock::PositionSettings{}.limit_y_down,
                "the written default INI's LimitYDown matches core's PositionSettings default");
    CHECK_MSG(cfg.ads_mode == hol_ht::kDefaultAdsMode,
              "the written default INI's AdsMode matches the mod's default slot");
}

}  // namespace

int main() {
    LimitYReachesBothBoundsWhenLimitYDownIsAbsent();
    AnExplicitLimitYDownStillWins();
    EveryAdsModeValueRoundTripsAndATypoFallsBackToTheDefault();
    AnImpossibleVirtualKeyFallsBackToTheDefault();
    ANonFiniteFloatFallsBackToTheDefault();
    WriteDefaultLeavesAnExistingIniAlone();
    WrittenDefaultIniMatchesTheCodeItDocuments();

    return hol_test::Report();
}
