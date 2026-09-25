// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// The differential test for the HeadTracking.ini conversion.
//
// Two readings of every input, and what may differ between them:
//
//   Oracle  the reader of the newest published build (the `dev` pre-release,
//           c24cc5a, core 9d86b01), compiled from its own sources (oracle_api.h)
//   Import  the frozen reader in src/legacy_config/
//
// Comparison 1, oracle against import, is what a player sees change that the
// conversion did not cause: commits since the published build that change how
// the file is read. Each one is listed below with its commit.
//
// Inputs: the published build's first-run file (it shipped no config and seeded
// none, so that is the file every player of it holds), no file, an empty file,
// and core's corpus of mutations of the first-run file.

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "legacy_config/legacy_config.h"
#include "oracle_api.h"
#include "test_harness.h"

#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/config/testing/ini_mutations.h"
#include "cameraunlock/tracking/tracking_mode.h"

namespace {

using cameraunlock::TrackingMode;
namespace cfg = cameraunlock::config;
namespace testing = cameraunlock::config::testing;

// ---- Provenance ------------------------------------------------------------
//
// Every source the oracle and the import compile, pinned by the SHA-256 of its
// bytes. The oracle's files are the published build's, taken with
// `git show c24cc5a:src/<file>` and `git -C cameraunlock-core show 9d86b01:<path>`.
// The core files both readers compile are hash-equal to 9d86b01's, so the
// readers differ only where the mod's own reader changed. The frozen import is
// pinned at the commit that froze it, so nothing edits it afterwards.

struct Pinned {
    const char* path;
    const char* sha256;
};

constexpr Pinned kPinned[] = {
    // The oracle: c24cc5a:src/...
    {"tests/config_differential/oracle/src/config.cpp", "2969f1c790b571e6ac4776afe238c3379113db076f859192ead5e974d10802fd"},
    {"tests/config_differential/oracle/src/config.h", "753b3d22f7cac5af8f4a1db6f1e248e025e404798921789bf1a72e21f0101b9a"},
    {"tests/config_differential/oracle/src/ads.h", "4798f62a3c0d88490be206fadee651226bd17a08897525f0fb4b7274c60d311d"},
    {"tests/config_differential/oracle/src/logging.h", "ae05e3e25016c6954e8ab2615163a32ce3bc0d74359d4861b715e33ad0078a6b"},
    // The oracle: 9d86b01:cpp/include/cameraunlock/ads/..., which core has changed or removed since.
    {"tests/config_differential/oracle/core/cameraunlock/ads/ads_blend.h", "bcc009fa97e0d8284a46ed5284ec0741ad3f1e8d1babe4be07bf45476351560a"},
    {"tests/config_differential/oracle/core/cameraunlock/ads/ads_fade.h", "00b80e59d261546dd50138759676f5a1b0d07681fa79a9b89be69797dc35c37f"},
    {"tests/config_differential/oracle/core/cameraunlock/ads/ads_mode.h", "94cd36b585e878e673566f602e9496417cb797970162fcc9c2af1e50e24358de"},
    {"tests/config_differential/oracle/core/cameraunlock/ads/entry_pose.h", "0c26c26fd3f6ba8307b731af391340e9286504ef157329c04883495f211cc870"},
    // Both readers: core at the pin, hash-equal to 9d86b01:cpp/...
    {"cameraunlock-core/cpp/include/cameraunlock/config/ini_reader.h", "a7ffb44210ff59672fa97e8e5feaa2cb3e81938fcc0334a384c68bc371b3857a"},
    {"cameraunlock-core/cpp/src/config/ini_reader.cpp", "e01515c2656aaf533bae4350dc45b702c3e3d4043935743dcc9bd5ea581fbe1c"},
    {"cameraunlock-core/cpp/include/cameraunlock/logging/file_log.h", "43bdd2ef8554c78e5f440333463750c13b95110fe672b0b6e273244df9e7d169"},
    {"cameraunlock-core/cpp/src/logging/file_log.cpp", "73c53c2baa06bbfebe8211f62678aa2b60cb95f604743d3686951ba56b87ea47"},
    {"cameraunlock-core/cpp/include/cameraunlock/data/position_settings.h", "b24dceb8e25475aebc5a468a5c7362a4a4e64204d183d1408525345f32f547f5"},
    {"cameraunlock-core/cpp/include/cameraunlock/math/smoothing_utils.h", "fc2146f8c585e5f610c7234e302f59de4945679cfa28ff479ca47477ec073f22"},
    {"cameraunlock-core/cpp/include/cameraunlock/math/angle_utils.h", "d7a905270933e3cb0c4c361d29d3fd701655498cbcd1875ea79d180468bdbe6a"},
    // The import: src/logging.h is c24cc5a's, the legacy folder is frozen.
    {"src/logging.h", "ae05e3e25016c6954e8ab2615163a32ce3bc0d74359d4861b715e33ad0078a6b"},
    {"src/legacy_config/legacy_config.h", "80c43c94e0ac871539b5946df2f88c33dd266f162bf231cf7c73086a5738b27a"},
    {"src/legacy_config/legacy_config.cpp", "6506ef94eaff3358edf70527657925df2c2497a58b0fad6cb34ca12024b75bcd"},
};

std::string ReadFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string Sha256Hex(const std::string& bytes) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        throw std::runtime_error("BCryptOpenAlgorithmProvider(SHA256) failed");
    }
    BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char digest[32] = {};
    const bool ok =
        BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0)) &&
        BCRYPT_SUCCESS(BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),
                                      static_cast<ULONG>(bytes.size()), 0)) &&
        BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0));
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) throw std::runtime_error("SHA-256 failed");
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    for (unsigned char b : digest) {
        out += kHex[b >> 4];
        out += kHex[b & 15];
    }
    return out;
}

void SourcesAreThePinnedOnes() {
    for (const Pinned& p : kPinned) {
        const std::string actual = Sha256Hex(ReadFileBytes(std::string(HOL_SOURCE_DIR) + "/" + p.path));
        if (actual != p.sha256) std::printf("  %s is %s\n", p.path, actual.c_str());
        CHECK_MSG(actual == p.sha256, p.path);
    }
}

// ---- Scratch folders ---------------------------------------------------------
//
// One folder per input: GetPrivateProfileString, which both readers sit on, is
// free to cache the file it last read.

class Scratch {
public:
    Scratch() {
        static unsigned s_next = 0;
        char temp[MAX_PATH] = {};
        GetTempPathA(MAX_PATH, temp);
        dir_ = std::string(temp) + "hol_ht_diff_" + std::to_string(GetCurrentProcessId()) + "_" +
               std::to_string(s_next++);
        if (!CreateDirectoryA(dir_.c_str(), nullptr)) {
            throw std::runtime_error("cannot create " + dir_ + ", error " + std::to_string(GetLastError()));
        }
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;
    ~Scratch() {
        WIN32_FIND_DATAA found;
        const HANDLE h = FindFirstFileA((dir_ + "\\*").c_str(), &found);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                const std::string file = dir_ + "\\" + found.cFileName;
                SetFileAttributesA(file.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(file.c_str());
            } while (FindNextFileA(h, &found));
            FindClose(h);
        }
        RemoveDirectoryA(dir_.c_str());
    }

    const std::string& dir() const { return dir_; }
    std::string ini() const { return dir_ + "\\HeadTracking.ini"; }

    void Write(const std::string& bytes) const {
        std::ofstream out(ini(), std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out) throw std::runtime_error("cannot write " + ini());
    }

private:
    std::string dir_;
};

// ---- What a reading does -------------------------------------------------------

std::uint32_t Bits(float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

enum Action { kToggle, kCycleMode, kYawMode, kAdsMode };
const char* const kActionNames[] = {"toggle", "cycle tracking mode", "yaw mode", "ADS mode"};

// One registered binding: the action, the virtual-key code, and the modifiers
// it needs (0, or Ctrl+Shift as cameraunlock::input::KeyModifiers spells it).
using Hotkey = std::tuple<int, int, unsigned>;
constexpr unsigned kPlain = 0;
constexpr unsigned kCtrlShift = 3;

// Everything a reading decides that the running mod acts on: the settings, the
// state at startup, and the bindings the poller registers.
struct Observed {
    int udp_port = 0;
    bool start_enabled = false;
    bool start_world_yaw = false;
    int start_mode = 0;
    float local_smoothing = 0;
    float remote_smoothing = 0;
    float limits[5] = {};
    bool aim_probe = false;
    std::vector<Hotkey> hotkeys;
};

// The sensitivities and inversions a legacy reader read, which the canonical
// format no longer has.
struct PoseShaping {
    float sensitivity[3] = {};
    bool invert[3] = {};
    float position_sensitivity[3] = {};
};

std::vector<std::string> Differences(const Observed& a, const Observed& b) {
    std::vector<std::string> out;
    if (a.udp_port != b.udp_port) out.push_back("[Network] Port");
    if (a.start_enabled != b.start_enabled) out.push_back("tracking on at startup");
    if (a.start_world_yaw != b.start_world_yaw) out.push_back("yaw mode at startup");
    if (a.start_mode != b.start_mode) out.push_back("tracking mode at startup");
    if (Bits(a.local_smoothing) != Bits(b.local_smoothing)) out.push_back("local smoothing");
    if (Bits(a.remote_smoothing) != Bits(b.remote_smoothing)) out.push_back("remote smoothing");
    static const char* const kLimits[] = {"LimitX", "LimitY", "LimitYDown", "LimitZ", "LimitZBack"};
    for (int i = 0; i < 5; ++i) {
        if (Bits(a.limits[i]) != Bits(b.limits[i])) out.push_back(kLimits[i]);
    }
    if (a.aim_probe != b.aim_probe) out.push_back("[Dev] AimProbe");
    if (a.hotkeys != b.hotkeys) out.push_back("hotkeys");
    return out;
}

bool SamePoseShaping(const PoseShaping& a, const PoseShaping& b) {
    for (int i = 0; i < 3; ++i) {
        if (Bits(a.sensitivity[i]) != Bits(b.sensitivity[i])) return false;
        if (a.invert[i] != b.invert[i]) return false;
        if (Bits(a.position_sensitivity[i]) != Bits(b.position_sensitivity[i])) return false;
    }
    return true;
}

// headtracking_mod.cpp ApplyConfigToSession, at c24cc5a and at the commit that
// froze the reader alike: [Position] Enabled picks the startup mode and nothing
// else.
int LegacyStartMode(bool position_enabled) {
    return static_cast<int>(position_enabled ? TrackingMode::RotationAndPosition : TrackingMode::RotationOnly);
}

// mod_hotkeys.cpp Register at the commit that froze the reader: End, Page Up and
// the configured yaw key NavGuarded, the Y/G/H chords ChordGuarded.
std::vector<Hotkey> LegacyHotkeys(int yaw_mode_key) {
    std::vector<Hotkey> keys = {
        {kToggle, 0x23, kPlain},      {kCycleMode, 0x21, kPlain},  {kYawMode, yaw_mode_key, kPlain},
        {kToggle, 0x59, kCtrlShift},  {kCycleMode, 0x47, kCtrlShift}, {kYawMode, 0x48, kCtrlShift},
    };
    std::sort(keys.begin(), keys.end());
    return keys;
}

struct OracleReading {
    Observed observed;
    PoseShaping pose;
    int ads_mode = 0;
};

OracleReading ReadOracle(const std::string& dir) {
    const hol_oracle::PublishedConfig c = hol_oracle::Load(dir);
    OracleReading r;
    r.observed.udp_port = c.udp_port;
    // view_hook.cpp Install at c24cc5a.
    r.observed.start_enabled = c.enable_on_startup;
    r.observed.start_world_yaw = c.world_space_yaw;
    r.observed.start_mode = LegacyStartMode(c.position_enabled);
    r.observed.local_smoothing = c.local_smoothing;
    r.observed.remote_smoothing = c.remote_smoothing;
    const float limits[5] = {c.limit_x, c.limit_y, c.limit_y_down, c.limit_z, c.limit_z_back};
    std::copy(std::begin(limits), std::end(limits), r.observed.limits);
    r.observed.aim_probe = c.aim_probe;
    // mod_hotkeys.cpp Register at c24cc5a: the frozen reader's set, plus the ADS
    // cycle on its configured key and Ctrl+Shift+U.
    r.observed.hotkeys = LegacyHotkeys(c.yaw_mode_key);
    r.observed.hotkeys.push_back({kAdsMode, c.ads_mode_key, kPlain});
    r.observed.hotkeys.push_back({kAdsMode, 0x55, kCtrlShift});
    std::sort(r.observed.hotkeys.begin(), r.observed.hotkeys.end());
    r.pose = {{c.yaw_sensitivity, c.pitch_sensitivity, c.roll_sensitivity},
              {c.invert_yaw, c.invert_pitch, c.invert_roll},
              {c.position_sensitivity_x, c.position_sensitivity_y, c.position_sensitivity_z}};
    r.ads_mode = c.ads_mode;
    return r;
}

Observed ObserveLegacy(const hol_ht::legacy::Config& c) {
    Observed o;
    o.udp_port = c.udp_port;
    o.start_enabled = c.enable_on_startup;
    o.start_world_yaw = c.world_space_yaw;
    o.start_mode = LegacyStartMode(c.position_enabled);
    o.local_smoothing = c.local_smoothing;
    o.remote_smoothing = c.remote_smoothing;
    const float limits[5] = {c.limit_x, c.limit_y, c.limit_y_down, c.limit_z, c.limit_z_back};
    std::copy(std::begin(limits), std::end(limits), o.limits);
    o.aim_probe = c.aim_probe;
    o.hotkeys = LegacyHotkeys(c.yaw_mode_key);
    return o;
}

PoseShaping LegacyPoseShapingOf(const hol_ht::legacy::Config& c) {
    return {{c.yaw_sensitivity, c.pitch_sensitivity, c.roll_sensitivity},
            {c.invert_yaw, c.invert_pitch, c.invert_roll},
            {c.position_sensitivity_x, c.position_sensitivity_y, c.position_sensitivity_z}};
}

// ---- Inputs --------------------------------------------------------------------

std::string DataPath(const char* name) {
    return std::string(HOL_SOURCE_DIR) + "/tests/config_differential/data/" + name;
}

// The published build's first-run file, extracted once from the oracle's
// WriteDefaultIfMissing and committed.
std::string FirstRunFile() { return ReadFileBytes(DataPath("dev-c24cc5a-first-run.ini")); }

// Every key the frozen reader reads, and how the corpus varies each one. The
// out-of-range values sit either side of the range each key is clamped or
// refused to.
std::vector<testing::MutationKey> CorpusKeys() {
    const std::vector<std::string> limit = {"0.001", "0.6"};
    return {
        {"Network", "Port", "5000", {"80", "70000"}},
        {"General", "EnableOnStartup", "false", {}},
        {"General", "WorldSpaceYaw", "false", {}},
        {"Sensitivity", "Yaw", "1.5", {"0.05", "3.5"}},
        {"Sensitivity", "Pitch", "0.5", {"0.05", "3.5"}},
        {"Sensitivity", "Roll", "2.25", {"0.05", "3.5"}},
        {"Inversion", "Yaw", "true", {}},
        {"Inversion", "Pitch", "true", {}},
        {"Inversion", "Roll", "true", {}},
        {"Smoothing", "Local", "0.3", {"-0.5", "1.5"}},
        {"Smoothing", "Remote", "0.6", {"-0.5", "1.5"}},
        {"Position", "Enabled", "false", {}},
        {"Position", "SensitivityX", "2.0", {"-1.0", "6.0"}},
        {"Position", "SensitivityY", "0.5", {"-1.0", "6.0"}},
        {"Position", "SensitivityZ", "1.25", {"-1.0", "6.0"}},
        {"Position", "LimitX", "0.25", limit},
        {"Position", "LimitY", "0.35", limit},
        {"Position", "LimitYDown", "0.15", limit},
        {"Position", "LimitZ", "0.45", limit},
        {"Position", "LimitZBack", "0.05", limit},
        {"Hotkeys", "YawMode", "0x2E", {"0x1FF"}, true},
        {"Dev", "AimProbe", "true", {}},
    };
}

std::vector<cfg::LegacyKey> CorpusReads() {
    std::vector<cfg::LegacyKey> reads;
    for (const testing::MutationKey& k : CorpusKeys()) reads.push_back({k.section, k.key});
    return reads;
}

struct Input {
    std::string name;
    bool present;
    std::string bytes;
};

std::vector<Input> Inputs() {
    std::vector<Input> inputs = {
        {"dev c24cc5a first-run file", true, FirstRunFile()},
        {"no file", false, {}},
        {"empty file", true, {}},
    };
    for (testing::IniMutation& m : testing::GenerateIniMutations(FirstRunFile(), CorpusReads(), CorpusKeys())) {
        inputs.push_back({"corpus: " + m.name, true, std::move(m.bytes)});
    }
    return inputs;
}

// ---- Checks --------------------------------------------------------------------

// The first-run file committed as test data is what the published build writes.
void FirstRunFileIsThePublishedBuilds() {
    Scratch s;
    hol_oracle::WriteDefaultIfMissing(s.dir());
    CHECK_MSG(ReadFileBytes(s.ini()) == FirstRunFile(),
              "dev-c24cc5a-first-run.ini is what the published build writes at first run");
}

// Comparison 1. What the published build did that the import does not, each
// with the commit that changed it:
//
// - b62aadc (feat: keep head tracking on through aim down sights) removed the
//   ADS mode cycle. The published build read [View] AdsMode (paused or tracked)
//   as the mode it started in, and [Hotkeys] AdsMode as the key that cycled it,
//   beside Ctrl+Shift+U. The import reads neither: head tracking stays on through
//   the aim, and Insert and Ctrl+Shift+U do nothing.
//
// Nothing else may differ, floats bit for bit.
void OracleAgainstImport(const std::vector<Input>& inputs) {
    int compared = 0;
    for (const Input& input : inputs) {
        Scratch s;
        if (input.present) s.Write(input.bytes);

        const OracleReading oracle = ReadOracle(s.dir());
        hol_ht::legacy::Config imported;
        hol_ht::legacy::Load(s.dir(), imported);
        const Observed import = ObserveLegacy(imported);

        Observed published = oracle.observed;
        const int ads_key = std::get<1>(*std::find_if(published.hotkeys.begin(), published.hotkeys.end(),
                                                      [](const Hotkey& h) {
                                                          return std::get<0>(h) == kAdsMode &&
                                                                 std::get<2>(h) == kPlain;
                                                      }));
        published.hotkeys.erase(std::remove_if(published.hotkeys.begin(), published.hotkeys.end(),
                                               [](const Hotkey& h) { return std::get<0>(h) == kAdsMode; }),
                                published.hotkeys.end());
        CHECK_MSG(ads_key >= 0x01 && ads_key <= 0xFE, "the published ADS key is a virtual-key code");
        CHECK_MSG(oracle.ads_mode == 0 || oracle.ads_mode == 2, "the published build started paused or tracked");

        const std::vector<std::string> diff = Differences(published, import);
        for (const std::string& d : diff) std::printf("  comparison 1, %s: %s\n", input.name.c_str(), d.c_str());
        CHECK_MSG(diff.empty(), "comparison 1: oracle and import agree apart from b62aadc's ADS keys");
        const bool same_pose = SamePoseShaping(oracle.pose, LegacyPoseShapingOf(imported));
        if (!same_pose) std::printf("  comparison 1, %s: pose shaping\n", input.name.c_str());
        CHECK_MSG(same_pose, "comparison 1: oracle and import read the same pose shaping");
        ++compared;
    }
    std::printf("comparison 1: %d inputs\n", compared);
}

}  // namespace

int main() {
    SourcesAreThePinnedOnes();
    FirstRunFileIsThePublishedBuilds();
    const std::vector<Input> inputs = Inputs();
    OracleAgainstImport(inputs);
    return hol_test::Report();
}
