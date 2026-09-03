// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "xs_widgets.h"

#include <atomic>
#include <cstdint>

#include <string>

#include "builds/build_registry.h"
#include "logging.h"

#include "cameraunlock/unreal/ue_runtime.h"
#include "cameraunlock/unreal/umg_reticle.h"

namespace hol_ht::xs_widgets {

namespace {

namespace ue = ::cameraunlock::unreal;

// What the walk looks for. The Image's own class and name, and a class name that
// has to appear up its outer chain - `Crosshair` alone is not unique, the main
// game's HUD has crosshair widgets of its own, and the widget blueprint's class
// default object carries a template copy that is never drawn.
constexpr const char* kWidgetClass = "Image";
constexpr const char* kWidgetName  = "Crosshair";
// Any of XenoSlaughter's own HUD widgets - XSHUDMaster_BP_C, XS_AmmoWidget_C
// and the rest all carry the prefix. Naming one of them exactly was a guess and
// it was wrong; the constraint that actually matters is that the Image belongs
// to the retro HUD rather than to the main game's, whose crosshair widgets are
// the HUD_DynamicCrosshair family, and this separates those two cleanly.
constexpr const char* kOwnerClass  = "XS";

// const FGeometry& UWidget::GetCachedGeometry() const.
using GetCachedGeometry_t = const void*(__fastcall*)(void* widget);


ue::UmgReticle g_reticle;
GetCachedGeometry_t g_getCachedGeometry = nullptr;
std::atomic<bool> g_bound{false};

// The HUD the crosshair belongs to. Its laid-out area is the retro picture, and
// it is measured rather than assumed - see PictureHalfExtent.
std::uintptr_t g_hudRoot = 0;


// What Slate says about one widget's rectangle.
//
// `scale` is FGeometry::Scale, the LAYOUT scale. It is not the whole
// local-to-absolute mapping and trusting it as though it were is what put the
// mark at the wrong distance: a widget inside a parent with a render transform
// carries that scale in AccumulatedRenderTransform and not in Scale, and Scale
// reads 1.0 the whole time. So the accumulated transform's diagonal is read as
// well, and that is what converts.
struct WidgetGeometry {
    float sizeX = 0.0f, sizeY = 0.0f;
    float scale = 0.0f;
    // AccumulatedRenderTransform's diagonal: local * accum = absolute.
    float accumX = 0.0f, accumY = 0.0f;
};

// FGeometry: Size +0x00, Scale +0x08, AbsolutePosition +0x0C, Position +0x14,
// AccumulatedRenderTransform +0x1C. That last is an FTransform2D - a 2x2 matrix
// then a translation - so its M[0][0] is at +0x1C and M[1][1] at +0x28.
bool ReadGeometryAt(std::uintptr_t geometry, WidgetGeometry& out) {
    if (geometry == 0) return false;
    if (!ue::SafeReadFloat(geometry + 0x00, out.sizeX) ||
        !ue::SafeReadFloat(geometry + 0x04, out.sizeY) ||
        !ue::SafeReadFloat(geometry + Offsets().kGeometryScaleOffset, out.scale) ||
        !ue::SafeReadFloat(geometry + 0x1C, out.accumX) ||
        !ue::SafeReadFloat(geometry + 0x28, out.accumY)) return false;
    // Slate hands out a zero geometry for a widget it has not laid out yet, and
    // every one of these is a divisor or a multiplier below.
    return out.scale > 0.0f && out.sizeX > 0.0f && out.sizeY > 0.0f &&
           out.accumX > 0.0f && out.accumY > 0.0f;
}

bool ReadGeometry(std::uintptr_t widget, WidgetGeometry& out) {
    if (!g_getCachedGeometry || widget == 0) return false;
    const void* geometry = g_getCachedGeometry(reinterpret_cast<void*>(widget));
    return ReadGeometryAt(reinterpret_cast<std::uintptr_t>(geometry), out);
}

// A widget's own name says nothing about which HUD it belongs to - every HUD in
// this game can hold an Image called Crosshair - so when the search fails, the
// chain is what says where the ones that exist actually live.
std::string OuterChain(std::uintptr_t obj) {
    std::string chain;
    std::uintptr_t cur = ue::OuterObject(obj);
    for (int depth = 0; depth < 8 && cur != 0; ++depth) {
        if (!chain.empty()) chain += " < ";
        chain += ue::ClassName(cur) + " " + ue::ObjectName(cur);
        cur = ue::OuterObject(cur);
    }
    return chain;
}

// Every Image named Crosshair that is live, with where it sits. Bounded, and
// only ever reached once, on the failure path - it is the evidence for choosing
// the owner constraint above rather than guessing at it a second time.
void LogEveryCandidate() {
    int listed = 0;
    ue::ForEachUObject([&](std::uintptr_t obj) {
        if (!ue::EqualsCI(ue::ClassName(obj), kWidgetClass)) return false;
        if (!ue::EqualsCI(ue::ObjectName(obj), kWidgetName)) return false;
        Log::Line("xs-widgets:   candidate obj=0x%llx  in %s",
                  static_cast<unsigned long long>(obj), OuterChain(obj).c_str());
        return ++listed >= 12;
    });
    if (listed == 0)
        Log::Line("xs-widgets:   no live %s named %s exists at all", kWidgetClass, kWidgetName);
}

}  // namespace

bool Install() {
    const std::uintptr_t rva = Offsets().kWidgetSetRenderTranslationRva;
    if (rva == 0) {
        Log::Line("xs-widgets: this build profile has no UWidget::SetRenderTranslation RVA - "
                  "the retro game's crosshair cannot be moved, so head tracking stays off for "
                  "that section");
        return false;
    }
    if (!g_reticle.Bind(ue::ModuleBase() + rva)) return false;

    if (Offsets().kWidgetGetCachedGeometryRva != 0) {
        g_getCachedGeometry = reinterpret_cast<GetCachedGeometry_t>(
            ue::ModuleBase() + Offsets().kWidgetGetCachedGeometryRva);
    } else {
        Log::Line("xs-widgets: this build profile has no UWidget::GetCachedGeometry RVA - the "
                  "crosshair's local-to-screen scale is unknown, so it would move the wrong "
                  "distance; head tracking stays off in the retro game");
        return false;
    }

    g_bound.store(true, std::memory_order_release);
    return true;
}

bool EnsureWidget() {
    if (!g_bound.load(std::memory_order_acquire)) return false;
    if (g_reticle.Widget() != 0) return true;

    const std::uintptr_t widget =
        ue::FindWidgetInstance(kWidgetClass, kWidgetName, kOwnerClass);
    if (widget == 0) {
        // Once per section entry, not once per frame: the widget legitimately
        // does not exist for the first frames of the section while its HUD is
        // still being built.
        static std::atomic<int> s_misses{0};
        if (s_misses.fetch_add(1, std::memory_order_relaxed) == 240) {
            Log::Line("xs-widgets: no live %s named %s under a %s* widget after 240 frames of "
                      "the retro game - head tracking stays off there, because its crosshair "
                      "cannot be moved to follow the camera. What does exist:",
                      kWidgetClass, kWidgetName, kOwnerClass);
            LogEveryCandidate();
        }
        return false;
    }

    g_reticle.SetWidget(widget);

    // The HUD root, for the picture's size. Walking up from the crosshair rather
    // than searching for it again ties the two together: whatever HUD this
    // crosshair is in is the HUD whose area the picture fills.
    g_hudRoot = 0;
    for (std::uintptr_t cur = ue::OuterObject(widget), depth = 0;
         cur != 0 && depth < 8; cur = ue::OuterObject(cur), ++depth) {
        if (ue::ContainsCI(ue::ClassName(cur), kOwnerClass)) { g_hudRoot = cur; break; }
    }
    // The chain, not just the pointer: it is the only line that proves the mark
    // being moved is the retro game's and not some other HUD's Crosshair.
    Log::Line("xs-widgets: holding the retro game's crosshair (obj=0x%llx, %s %s in %s) - it "
              "moves with the camera now",
              static_cast<unsigned long long>(widget), kWidgetClass, kWidgetName,
              OuterChain(widget).c_str());
    return true;
}

bool PictureHalfExtent(float& halfWidth, float& halfHeight) {
    WidgetGeometry cross, hud;
    if (!ReadGeometry(g_reticle.Widget(), cross)) return false;
    if (!ReadGeometry(g_hudRoot, hud)) return false;

    // MEASURED. This fraction cannot be derived from anything the mod can read,
    // and every attempt to derive it overcompensated - which is the whole
    // history of this file.
    //
    // Two independent measurements, taken in game on 2026-09-01:
    //   - commanding a fixed 100-unit offset moved the crosshair 222.5 screen
    //     px, so one HUD unit is 2.225 px;
    //   - the WORLD moves 65 px across for a head yaw of 8 degrees and 64 px
    //     down for the same pitch, measured by correlating two screenshots.
    //     Equal, as a pinhole camera with square pixels must be - so any
    //     asymmetry left in the mark is the HUD's stretch, not the picture's.
    // So the mark must move 65/2.225 = 29.2 units for tan(8deg) = 0.1405, i.e.
    // 207.9 units per unit of tangent. The runtime asks for ndc = tan * m00 with
    // m00 = 0.6326, so the half-width is 207.9/0.6326 = 328.6 units - which is
    // 0.2857 of the HUD's 1150.
    //
    // Why it cannot be computed: the capture renders at FOVAngle 115.37 (read
    // off the component, before AND after the engine's own call), but the
    // picture as displayed measures about 140 degrees across. The game composits
    // that render target at a scale of its own, and nothing in the capture
    // component or in Slate exposes it. The HUD spans the full width of what is
    // drawn; the 3D picture inside it does not.
    //
    // Expressed as a fraction of the HUD rather than in pixels because the HUD is
    // a fixed design canvas - 1150x700 - scaled to whatever the display is, so
    // the fraction is the part that stays true at another resolution. If a build
    // changes it, re-measure it the same way; do not re-derive it.
    // The same fraction of the HUD in BOTH axes, which is the form that survives
    // the HUD's stretch. That stretch is NOT uniform - measured at 2.225 screen
    // px per unit across and 2.003 down, a 1150x700 canvas filling a 2560x1440
    // area - so a height derived from the width by the picture's 16:9 aspect is
    // wrong by that same 11%, which showed up as pitch running 8.6% short of the
    // world while yaw was exact. Taking each extent against its own HUD
    // dimension absorbs it.
    constexpr float kPictureFractionOfHud = 0.287f;
    const float pictureAbsW = hud.sizeX * hud.accumX * kPictureFractionOfHud * 2.0f;
    const float pictureAbsH = hud.sizeY * hud.accumY * kPictureFractionOfHud * 2.0f;

    // Into the crosshair's local units, by the ACCUMULATED transform rather than
    // by Scale: a parent that scales its children carries the factor there, and
    // Scale reads 1.0 throughout.
    halfWidth = pictureAbsW * 0.5f / cross.accumX;
    halfHeight = pictureAbsH * 0.5f / cross.accumY;

    static std::atomic<bool> s_logged{false};
    if (!s_logged.exchange(true)) {
        Log::Line("xs-widgets: HUD size=(%.1f,%.1f) accum=(%.3f,%.3f); crosshair accum=(%.3f,%.3f); "
                  "picture %.1fx%.1f in HUD units (a measured fraction of the HUD, both axes), so "
                  "half-extents are %.1f x %.1f",
                  hud.sizeX, hud.sizeY, hud.accumX, hud.accumY, cross.accumX, cross.accumY,
                  pictureAbsW, pictureAbsH, halfWidth, halfHeight);
    }
    return true;
}

bool Move(float screenDx, float screenDy) { return g_reticle.MoveTo(screenDx, screenDy); }

bool Ready() { return g_reticle.Ready(); }

void Forget() {
    if (g_reticle.Widget() == 0) return;
    // Put it back before letting go, or the widget is destroyed carrying an
    // offset and the next section starts with the mark off-centre.
    g_reticle.MoveTo(0.0f, 0.0f);
    g_reticle.SetWidget(0);
}

}  // namespace hol_ht::xs_widgets
