// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "rva_hook.h"

#include "logging.h"

#include "cameraunlock/hooks/hook_manager.h"
#include "cameraunlock/unreal/ue_runtime.h"

namespace hol_ht {

bool InstallRvaHook(const char* channel, const char* name, std::uintptr_t rva,
                    void* detour, void** original) {
    namespace hooks = ::cameraunlock::hooks;

    auto& manager = hooks::HookManager::Instance();
    void* target = reinterpret_cast<void*>(::cameraunlock::unreal::ModuleBase() + rva);

    if (auto s = manager.CreateHook(target, detour, original); s != hooks::HookStatus::Ok) {
        Log::Line("%s CreateHook(%s) failed: %s", channel, name,
                  hooks::HookStatusToString(s));
        return false;
    }
    if (auto s = manager.EnableHook(target); s != hooks::HookStatus::Ok) {
        Log::Line("%s EnableHook(%s) failed: %s", channel, name,
                  hooks::HookStatusToString(s));
        // The trampoline is allocated and the manager is tracking it. Leaving it
        // there hands a live detour to a caller that has been told the hook does
        // not exist.
        manager.RemoveHook(target);
        return false;
    }
    return true;
}

}  // namespace hol_ht
