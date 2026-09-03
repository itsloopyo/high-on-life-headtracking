// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// Bootstrap: everything that has to happen once, in order, before the hook can
// run - log, crash handler, config, build-profile fingerprint, module range,
// UDP receiver, tracking session, hooks, hotkeys. It runs on its own thread so
// none of it happens under the loader lock.

#include "headtracking_mod.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>
#include <psapi.h>

#include "aim_point.h"
#include "builds/build_registry.h"
#include "config.h"
#include "logging.h"
#include "mod_hotkeys.h"
#include "session.h"
#include "view_hook.h"

#include "cameraunlock/diagnostics/crash_handler.h"
#include "cameraunlock/hooks/hook_manager.h"
#include "cameraunlock/unreal/ue_runtime.h"

namespace hol_ht {

namespace {

namespace ue = ::cameraunlock::unreal;

using cameraunlock::TrackingMode;

Config g_config;
std::unique_ptr<cameraunlock::UdpReceiver> g_receiver;
std::unique_ptr<Session> g_session;

void ApplyConfigToSession() {
    cameraunlock::SensitivitySettings sens;
    sens.yaw          = g_config.yaw_sensitivity;
    sens.pitch        = g_config.pitch_sensitivity;
    sens.roll         = g_config.roll_sensitivity;
    sens.invert_yaw   = g_config.invert_yaw;
    sens.invert_pitch = g_config.invert_pitch;
    sens.invert_roll  = g_config.invert_roll;
    g_session->GetProcessor().SetSensitivity(sens);

    auto& ps = g_session->GetPositionProcessor().GetSettings();
    ps.sensitivity_x = g_config.position_sensitivity_x;
    ps.sensitivity_y = g_config.position_sensitivity_y;
    ps.sensitivity_z = g_config.position_sensitivity_z;
    ps.limit_x       = g_config.limit_x;
    ps.limit_y       = g_config.limit_y;
    ps.limit_y_down  = g_config.limit_y_down;
    ps.limit_z       = g_config.limit_z;
    ps.limit_z_back  = g_config.limit_z_back;

    // The session feeds both the rotation and the position processor - there is
    // no separate position smoothing setting - and picks between the two values
    // per connection from the receiver's IsRemoteConnection(), re-read on every
    // Update().
    static_assert(Session::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection() or smoothing silently stays local");
    g_session->SetLocalSmoothing(g_config.local_smoothing);
    g_session->SetRemoteSmoothing(g_config.remote_smoothing);

    g_session->SetMode(g_config.position_enabled
        ? TrackingMode::RotationAndPosition
        : TrackingMode::RotationOnly);
}

// GetModuleFileNameW does two things a fixed MAX_PATH buffer gets wrong. It
// leaves the buffer INDETERMINATE when it fails, so building a wstring from it
// reads uninitialised stack until it happens on a NUL. And when the path does
// not fit it TRUNCATES rather than failing - which is not theoretical on
// Windows, where a Steam library can sit well past MAX_PATH - and a truncated
// directory sends HeadTracking.log and HeadTracking.ini somewhere that is not
// next to the exe, so the player gets no log and their hand-edited settings are
// ignored with nothing saying why.
//
// So the buffer grows until the whole path fits. The failure return is empty,
// which the two callers below already turn into ".".
std::wstring ExePath() {
    // The Win32 path ceiling: 32767 wide characters plus the terminator.
    constexpr std::size_t kMaxPathChars = 32768;
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) return std::wstring();
        // A short return means the whole path fit. Windows signals truncation by
        // filling the buffer exactly, so n == size is the grow case.
        if (n < buf.size()) return std::wstring(buf.data(), n);
        if (buf.size() >= kMaxPathChars) return std::wstring();
        buf.resize(buf.size() * 2);
    }
}

std::wstring ExeDir() {
    const std::wstring path = ExePath();
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

// Narrow sibling of ExeDir for the ANSI IniReader (GetPrivateProfile*A).
// Converted from the wide path through the ANSI code page, which is what
// GetModuleFileNameA does internally - so this is the same string that call
// produced, without a second fixed buffer to truncate.
std::string ExeDirNarrow() {
    const std::wstring dir = ExeDir();
    const int needed = WideCharToMultiByte(CP_ACP, 0, dir.c_str(), static_cast<int>(dir.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return ".";
    std::string narrow(static_cast<std::size_t>(needed), 0);
    WideCharToMultiByte(CP_ACP, 0, dir.c_str(), static_cast<int>(dir.size()),
                        narrow.data(), needed, nullptr, nullptr);
    return narrow;
}

void OpenLog() {
    // Core opens with CREATE_ALWAYS, so the log holds this session only, and
    // rotates the outgoing one to HeadTracking.prev.log first - the session
    // worth reading is usually the one that just crashed, and the user
    // relaunches the game before sending the file. It warns by itself when the
    // rotation fails, so there is nothing to do here but name the file.
    Log::Open(ExeDir() + L"\\HeadTracking.log");
    Log::Line("=== High On Life Head Tracking v" HOL_HT_VERSION " (UE4) ===");
}

void LoadConfig() {
    const std::string exeDir = ExeDirNarrow();
    config::WriteDefaultIfMissing(exeDir);
    config::Load(exeDir, g_config);
    Log::Line("config: udp_port=%d enable=%d yaw_sens=%.2f smoothing=local %.2f/remote %.2f position=%d",
        g_config.udp_port, g_config.enable_on_startup ? 1 : 0,
        g_config.yaw_sensitivity, g_config.local_smoothing, g_config.remote_smoothing,
        g_config.position_enabled ? 1 : 0);
}

// Fingerprint the host EXE against the build registry. False leaves the mod
// fully dormant: no hooks installed, game runs vanilla.
bool SelectBuildProfile(HMODULE host) {
    // SelectProfile() has already logged the running fingerprint, every profile
    // it compared against, and the name of the one it settled on; what is left
    // to say is what a player should do about a build with no profile.
    switch (builds::SelectProfile(host)) {
        case builds::MatchResult::Matched:
            return true;
        case builds::MatchResult::HostNewer:
            Log::Line("build-check: this game build is NEWER than any profile this "
                      "mod knows about - check the releases page for an update. "
                      "Staying dormant; game runs vanilla.");
            return false;
        case builds::MatchResult::HostOlder:
            Log::Line("build-check: this game build is OLDER than the profile - let "
                      "Steam finish updating. Staying dormant; game runs vanilla.");
            return false;
        case builds::MatchResult::HostDiffers:
            Log::Line("build-check: same build date but a different EXE - this mod "
                      "does not engage on a modified binary. Staying dormant; game "
                      "runs vanilla.");
            return false;
        case builds::MatchResult::ProfileUnusable:
            // SelectProfile has already said what is wrong with it, and it is
            // our bug rather than the player's.
            return false;
        case builds::MatchResult::ReadFailed:
            Log::Line("build-check: could not read the PE header - staying dormant; "
                      "game runs vanilla.");
            return false;
    }
    return false;
}

// Hand the shared UE runtime the module range every RVA is resolved against.
// The layout is left zeroed: this mod reads its offsets from the build profile
// and never walks GUObjectArray, so there is no FName/UObject reflection to
// configure.
bool PublishModuleRange(HMODULE host) {
    MODULEINFO mi{};
    if (!GetModuleInformation(GetCurrentProcess(), host, &mi, sizeof(mi))) {
        Log::Line("FATAL: GetModuleInformation failed - cannot resolve RVAs");
        return false;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(mi.lpBaseOfDll);
    // The real layout, not an empty one: without it core's reflection is inert,
    // and the retro crosshair is found by walking GUObjectArray.
    ue::SetRuntime(base, base + mi.SizeOfImage, Offsets().UObjectGlobals);
    Log::Line("module base=0x%llx size=0x%x",
        static_cast<unsigned long long>(base), mi.SizeOfImage);
    return true;
}

// A port that is busy right now is not a failure, so nothing here reacts to
// Start()'s return value beyond saying which of the two states the mod is in.
//
// Start() has already spawned the receiver's supervisor thread, and that thread
// re-attempts the bind every UdpReceiver::kRetryIntervalMs for as long as the
// receiver lives - so a player who launches High On Life with their previous
// game (or a second tracker consumer) still holding the port gets tracking the
// moment they close it, without touching the mod or restarting the game. It
// also re-establishes the socket if the receive thread dies on a socket error,
// which is the one state that never recovers on its own. Both transitions are
// logged by the receiver through the sink installed below.
//
// Bailing out on a false return - `if (!Start(port)) return;` - is what this
// must never become. It leaves the view hook uninstalled and the hotkeys
// unregistered on a mod that would otherwise have come back on its own, and the
// player has no way to tell that from a mod that failed outright.
void StartTracking() {
    g_receiver = std::make_unique<cameraunlock::UdpReceiver>();
    g_receiver->SetLog([](const std::string& m) { Log::Line("udp: %s", m.c_str()); });

    if (!g_receiver->Start(static_cast<uint16_t>(g_config.udp_port))) {
        Log::Line("udp: port %d is busy - the mod is loading anyway and will start "
                  "listening within %dms of it being freed. Close whatever is holding "
                  "it (another game's head tracking mod, or a second tracker app) and "
                  "tracking comes up on its own.",
            g_config.udp_port, cameraunlock::UdpReceiver::kRetryIntervalMs);
    }

    g_session = std::make_unique<Session>(*g_receiver);
    ApplyConfigToSession();
}

DWORD WINAPI BootstrapThread(LPVOID) {
    OpenLog();
    cameraunlock::diagnostics::InstallCrashHandler();
    LoadConfig();

    HMODULE host = GetModuleHandleW(nullptr);
    if (!SelectBuildProfile(host)) return 0;
    if (!PublishModuleRange(host)) return 0;

    StartTracking();

    auto& hm = cameraunlock::hooks::HookManager::Instance();
    if (auto s = hm.Initialize(); s != cameraunlock::hooks::HookStatus::Ok) {
        Log::Line("FATAL: MinHook init failed: %s", cameraunlock::hooks::HookStatusToString(s));
        return 0;
    }

    if (!view_hook::Install({&g_config, g_session.get(), g_receiver.get()}))
        return 0;

    // After the view hook, and never on the path between a drawing consumer and
    // it: this one runs to completion before the crosshair's projection starts,
    // so it is off the stack the presentation gate walks.
    aim_point::Install(g_config.aim_probe);

    hotkeys::Register(g_config);

    Log::Line("init complete. End=toggle PageUp=trackingmode VK 0x%02X=yawmode (%s) "
              "VK 0x%02X=adsmode (chords Ctrl+Shift+Y/G/H/U). UDP %d: %s.",
        g_config.yaw_mode_key, g_config.world_space_yaw ? "world" : "local",
        g_config.ads_mode_key, g_config.udp_port,
        g_receiver->IsRunning() ? "listening" : "waiting for the port to free up");
    return 0;
}

}  // namespace

void Initialize(HMODULE self) {
    // Pin the module. Ultimate ASI Loader never frees a plugin, but nothing
    // stops something else calling FreeLibrary, and an unload is unsurvivable
    // here: MinHook's thread freeze only relocates instruction pointers sitting
    // in a trampoline, so a game thread parked inside one of our detours is
    // invisible to it and returns into unmapped memory. Refusing to unload is
    // the only version of this that is safe, and it is why there is no
    // Shutdown() - tearing hooks down from DllMain would also have to join the
    // hotkey and UDP threads under the loader lock, which is a deadlock.
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(self), &pinned);

    const HANDLE thread = CreateThread(nullptr, 0, BootstrapThread, nullptr, 0, nullptr);
    if (!thread) {
        // The log does not exist yet - OpenLog runs on the thread that just
        // failed to start - so this is the only channel left. Without it a dead
        // mod and an ASI that never loaded look identical: no HeadTracking.log
        // either way.
        OutputDebugStringW(L"[HighOnLifeHeadTracking] CreateThread failed; the mod is not running.");
        return;
    }
    CloseHandle(thread);
}

}  // namespace hol_ht
