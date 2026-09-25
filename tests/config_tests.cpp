// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// HeadTracking.ini in the canonical config format.
//
// The committed config/HeadTracking.ini is the file the table renders from its
// defaults, which is also what the owner creates at first launch. A toggle's
// save changes the lines of its rows and no other byte. An older file is
// converted through the frozen import; tests/config_differential/ holds that to
// the published build over the whole corpus, and the cases here are the ones
// worth reading as examples.
//
// `hol_config_tests --render-config <path>` writes the rendered defaults to
// <path> and exits, which is how `pixi run render-config` rewrites the committed
// file after a change to a row, a comment or a default.

#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <windows.h>

#include "config.h"
#include "test_harness.h"

namespace {

namespace cfg = ::cameraunlock::config;
using cameraunlock::TrackingMode;

std::string ReadFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteFileBytes(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("cannot write " + path);
}

std::string Rendered() {
    const cfg::ConfigTable<hol_ht::Config> table = hol_ht::config::Table();
    return cfg::RenderCanonical(table, table.defaults(), hol_ht::config::Header());
}

std::string CommittedFile() { return ReadFileBytes(std::string(HOL_SOURCE_DIR) + "/config/HeadTracking.ini"); }

// A folder of its own per case, emptied and removed afterwards.
class Scratch {
public:
    explicit Scratch(const char* tag) {
        char temp[MAX_PATH] = {};
        GetTempPathA(MAX_PATH, temp);
        dir_ = std::string(temp) + "hol_ht_config_" + tag + "_" + std::to_string(GetCurrentProcessId());
        if (!CreateDirectoryA(dir_.c_str(), nullptr)) throw std::runtime_error("cannot create " + dir_);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;
    ~Scratch() {
        WIN32_FIND_DATAA found;
        const HANDLE h = FindFirstFileA((dir_ + "\\*").c_str(), &found);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) DeleteFileA((dir_ + "\\" + found.cFileName).c_str());
            } while (FindNextFileA(h, &found));
            FindClose(h);
        }
        RemoveDirectoryA(dir_.c_str());
    }

    std::wstring wdir() const { return std::wstring(dir_.begin(), dir_.end()); }
    std::string ini() const { return dir_ + "\\HeadTracking.ini"; }

private:
    std::string dir_;
};

std::vector<std::string> Lines(const std::string& bytes) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t end; (end = bytes.find("\r\n", start)) != std::string::npos; start = end + 2) {
        lines.push_back(bytes.substr(start, end - start));
    }
    return lines;
}

// The lines that differ between two files of the same line count, or "count" when
// the counts differ.
std::vector<std::string> ChangedLines(const std::string& before, const std::string& after) {
    const std::vector<std::string> a = Lines(before), b = Lines(after);
    if (a.size() != b.size()) return {"count"};
    std::vector<std::string> changed;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) changed.push_back(b[i]);
    }
    return changed;
}

void TheCommittedFileIsTheRenderedDefaults() {
    CHECK_MSG(Rendered() == CommittedFile(),
              "config/HeadTracking.ini is the table's render of its defaults; run pixi run render-config");
}

void FirstLaunchCreatesTheCommittedFile() {
    Scratch s("created");
    const hol_ht::Config loaded = hol_ht::config::Load(s.wdir());
    CHECK_MSG(ReadFileBytes(s.ini()) == CommittedFile(), "the first launch writes the committed file byte for byte");
    CHECK(loaded.toggle_key == "End, Ctrl+Shift+Y");
    CHECK(loaded.cycle_tracking_mode_key == "PageUp, Ctrl+Shift+G");
    CHECK(loaded.yaw_mode_key == "PageDown, Ctrl+Shift+H");
}

void TheYawToggleSavesItsLineAndNothingElse() {
    Scratch s("save_yaw");
    hol_ht::config::Load(s.wdir());
    const std::string before = ReadFileBytes(s.ini());
    hol_ht::config::SaveWorldSpaceYaw(false);
    const std::vector<std::string> changed = ChangedLines(before, ReadFileBytes(s.ini()));
    CHECK_MSG(changed == std::vector<std::string>{"WorldSpaceYaw=false"}, "a yaw save changes WorldSpaceYaw alone");
    CHECK_MSG(!hol_ht::config::Load(s.wdir()).world_space_yaw, "the saved yaw mode comes back at the next launch");
}

void TheModeCycleSavesThePair() {
    Scratch s("save_mode");
    hol_ht::config::Load(s.wdir());
    const std::string before = ReadFileBytes(s.ini());

    hol_ht::config::SaveTrackingMode(TrackingMode::RotationOnly);
    CHECK_MSG(ChangedLines(before, ReadFileBytes(s.ini())) == std::vector<std::string>{"PositionEnabled=false"},
              "rotation only changes PositionEnabled alone");

    hol_ht::config::SaveTrackingMode(TrackingMode::PositionOnly);
    CHECK_MSG(ChangedLines(before, ReadFileBytes(s.ini())) == std::vector<std::string>{"RotationEnabled=false"},
              "position only writes the pair: RotationEnabled false, PositionEnabled back to true");
    const hol_ht::Config reloaded = hol_ht::config::Load(s.wdir());
    CHECK_MSG(!reloaded.rotation_enabled && reloaded.position_enabled, "the saved mode comes back at the next launch");

    hol_ht::config::SaveTrackingMode(TrackingMode::RotationAndPosition);
    CHECK_MSG(ReadFileBytes(s.ini()) == before, "back to full, the file is as it was created");
}

// An INI written before LimitYDown existed carries only LimitY, and the old
// reader gave LimitYDown LimitY's value. The conversion writes both.
void AnOldLimitYReachesBothBounds() {
    Scratch s("limit_y");
    WriteFileBytes(s.ini(), "[Position]\r\nLimitY=0.40\r\n");
    const hol_ht::Config c = hol_ht::config::Load(s.wdir());
    CHECK(c.limit_y == 0.4f);
    CHECK(c.limit_y_down == 0.4f);
    const std::string migrated = ReadFileBytes(s.ini());
    CHECK(migrated.find("\r\nPositionLimitY=0.4\r\n") != std::string::npos);
    CHECK(migrated.find("\r\nPositionLimitYDown=0.4\r\n") != std::string::npos);
    CHECK_MSG(ReadFileBytes(s.ini() + ".pre-canonical") == "[Position]\r\nLimitY=0.40\r\n",
              "the old file is kept as HeadTracking.ini.pre-canonical");
}

// The yaw key was the one hotkey in the old file. It joins its chord in the list;
// End and Page Up, bound in code before, are written out with theirs.
void AnOldYawKeyJoinsItsChord() {
    Scratch s("yaw_key");
    WriteFileBytes(s.ini(), "[View]\r\nAdsMode=tracked\r\n[Hotkeys]\r\nYawMode=0x2E\r\nAdsMode=0x2D\r\n");
    const hol_ht::Config c = hol_ht::config::Load(s.wdir());
    CHECK(c.yaw_mode_key == "Delete, Ctrl+Shift+H");
    CHECK(c.toggle_key == "End, Ctrl+Shift+Y");
    CHECK(c.cycle_tracking_mode_key == "PageUp, Ctrl+Shift+G");
    CHECK_MSG(ReadFileBytes(s.ini()).find("AdsMode") == std::string::npos, "the retired ADS keys are not carried");
}

// [Position] Enabled=false started the mod in rotation only, and the cycle still
// reached the position modes, so it is the startup mode and not a feature switch.
void AnOldPositionSwitchIsTheStartupMode() {
    Scratch s("position");
    WriteFileBytes(s.ini(), "[Position]\r\nEnabled=false\r\n");
    const hol_ht::Config c = hol_ht::config::Load(s.wdir());
    CHECK(c.rotation_enabled);
    CHECK(!c.position_enabled);
}

// Sensitivities and inversions shipped as identity, so nothing is folded, and one
// a player changed is not carried.
void AChangedSensitivityIsNotCarried() {
    Scratch s("sensitivity");
    WriteFileBytes(s.ini(), "[Sensitivity]\r\nYaw=2.50\r\n[Inversion]\r\nPitch=true\r\n[Smoothing]\r\nRemote=0.40\r\n");
    const hol_ht::Config c = hol_ht::config::Load(s.wdir());
    CHECK(c.remote_smoothing == 0.4f);
    const std::string migrated = ReadFileBytes(s.ini());
    CHECK(migrated.find("[Sensitivity]") == std::string::npos);
    CHECK(migrated.find("[Inversion]") == std::string::npos);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--render-config") == 0) {
        WriteFileBytes(argv[2], Rendered());
        return 0;
    }

    TheCommittedFileIsTheRenderedDefaults();
    FirstLaunchCreatesTheCommittedFile();
    TheYawToggleSavesItsLineAndNothingElse();
    TheModeCycleSavesThePair();
    AnOldLimitYReachesBothBounds();
    AnOldYawKeyJoinsItsChord();
    AnOldPositionSwitchIsTheStartupMode();
    AChangedSensitivityIsNotCarried();

    return hol_test::Report();
}
