// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include <cameraunlock/memory/pe_fingerprint.h>
#include <cameraunlock/unreal/ue_runtime.h>

// One BuildProfile describes a single shipped build of High On Life: the
// PE-header fingerprint that uniquely identifies it, plus every per-build RVA
// and field offset the mod needs. At startup the mod fingerprints the live
// module and selects the matching profile. No match leaves the mod fully
// dormant - no hooks installed, game runs vanilla. See AGENTS.md "Maintain
// compatibility across new patches": never edit an existing profile's RVAs in
// place, ADD a new one.

namespace hol_ht {

// PE-header build fingerprint (TimeDateStamp + SizeOfImage + CheckSum);
// the shared type keeps reading/matching/classification in core.
using PeFingerprint = ::cameraunlock::memory::PeFingerprint;

// Room for every drawing caller of the view, with slack for the ones a
// future build adds. Trailing zeroes are unused.
inline constexpr std::size_t kPresentationCallerSlots = 12;

// Player-controller vtable RVAs the gameplay gate recognises. A controller
// whose vtable is not one of these is not a class whose gameplay flags this
// profile has offsets for, so tracking stays off for it.
inline constexpr std::size_t kGameplayControllerVtableSlots = 4;

// Player-character vtable RVAs the ADS gate recognises, same guard as the
// controller list above: a Character whose vtable is not one of these is not
// a class ADSOn was read from.
inline constexpr std::size_t kPlayerCharacterVtableSlots = 4;

struct OffsetTable {
    // Hook target: APlayerController::GetPlayerViewPoint, the one place the
    // head pose reaches the game. Zero = profile incomplete (stay dormant).
    std::uintptr_t kGetPlayerViewPointRva;

    // Return-address RVA of the single GetPlayerViewPoint call inside
    // ULocalPlayer::GetViewPoint. Everything that reaches
    // GetPlayerViewPoint from anywhere else - interaction traces, the audio
    // listener, AI perception, replication - keeps the clean mouse/pad
    // rotation, and that gate IS the look/aim decoupling.
    std::uintptr_t kViewPointCallerRva;

    // The presentation gate, and why the gate above is not enough on its
    // own. ULocalPlayer::GetViewPoint is reached from two engine functions,
    // CalcSceneView and GetProjectionData, and GetProjectionData serves
    // BOTH the frame being drawn and every world-to-screen projection the
    // game asks for. Some of those projections are gameplay decisions -
    // where an encounter may spawn, what the chainsaw may grapple, what the
    // gaze component thinks the player can see - and injecting the head
    // pose into them would let head movement change what the game does.
    //
    // So when the hook fires at kViewPointCallerRva it walks a few frames
    // of the stack and injects only if one of these return addresses is on
    // it. Each entry is the instruction after a call, in the function that
    // wanted the view, so it names the CONSUMER rather than the plumbing.
    // Anything not listed projects through the clean view.
    std::array<std::uintptr_t, kPresentationCallerSlots> kPresentationCallerRvas;

    // How many stack frames above the hook to look at. The deepest listed
    // consumer sits five frames up (GetViewPoint, GetProjectionData,
    // ProjectWorldToScreen, ProjectWorldLocationToScreen, the caller), so
    // this has slack over that and no more - every extra frame is unwinding
    // work done per call.
    std::uint32_t kPresentationStackFrames;

    // GFrameCounter, the engine's uint64 game-thread frame number. The pose
    // is sampled once per frame and reused for every presentation caller in
    // that frame, so the crosshair projection and the frame it is drawn
    // over cannot be built from two different head poses. Zero falls back
    // to sampling per call, which is visibly worse and is logged.
    std::uintptr_t kGFrameCounterRva;

    // APlayerController::bShowMouseCursor, as a byte offset into the
    // controller plus the bit within the dword there. UE raises that flag
    // when input belongs to a menu rather than the player.
    std::size_t   kShowMouseCursorOffset;
    std::uint32_t kShowMouseCursorMask;

    // The game's own flags on AORPlayerController, both plain bools.
    // bPauseMenuActive covers the pause menu; bCinematicModeActive covers
    // scripted cinematics, which are composed shots and are left alone.
    std::size_t kPauseMenuActiveOffset;
    std::size_t kCinematicModeActiveOffset;

    // Primary vtable RVAs of the player-controller classes the two offsets
    // above were read from. The hook holds the controller pointer already,
    // so this costs one guarded load and rules out reading those bytes off
    // a class that does not have them.
    std::array<std::uintptr_t, kGameplayControllerVtableSlots> kGameplayControllerVtableRvas;

    // FMinimalViewInfo field offsets. ULocalPlayer::GetViewPoint hands
    // GetPlayerViewPoint pointers to the Location and Rotation fields of
    // the FMinimalViewInfo it is filling in, so the live FOV sits at
    // outLocation + kFovOffset. kRotationStride is the Location->Rotation
    // gap, checked against the actual out-param pair before the FOV is
    // read - if the two pointers are not that far apart they are not fields
    // of one FMinimalViewInfo and what looks like the FOV is a local in
    // some other caller's stack frame.
    struct {
        std::size_t kFovOffset;
        std::size_t kRotationStride;
    } MinimalViewInfoLayout;

    // Aiming down sights, read off the controller the hook already holds.
    // AController::Character is the ACharacter the controller is driving;
    // AORCharacter::ADSOn is the game's own one-byte answer, written by
    // AORCharacter::SetADSState and read by the 1P weapon anim instance and
    // by the input component's ADS sensitivity. The vtable list is the same
    // guard the gameplay flags get: a Character that is not one of these
    // classes is not one ADSOn was read from.
    std::size_t kCharacterOffset;
    std::size_t kAdsOnOffset;
    std::array<std::uintptr_t, kPlayerCharacterVtableSlots> kPlayerCharacterVtableRvas;

    // Where the crosshair's world aim point comes from, and the one value in
    // the game this mod substitutes. USQFiringResultComponent::GetAimLocation
    // is the non-virtual dispatcher every firing result goes through; the
    // return RVA is the single call to it inside
    // AORPlayerCharacter::UpdateCrosshairLocation, which passes
    // bUseAimCorrections = false and so gets a fixed ray end. Setting the
    // flag at that one call site runs the game's own aim trace and returns
    // the impact point instead - see aim_point.h. Zero leaves the crosshair
    // on the ray end.
    std::uintptr_t kGetAimLocationRva;
    std::uintptr_t kCrosshairAimCallerRva;

    // The retro game at the start, which is drawn by a camera that is not the
    // player's. AORXSPlayerCharacter::UpdateSceneCapture builds an
    // FMinimalViewInfo for the USceneCaptureComponent2D whose render target the
    // screen in the room displays, and hands it to
    // USceneCaptureComponent2D::SetCameraView - that function's only caller in
    // this build. Hooking it puts the head pose on the camera that draws the
    // retro game instead of the room view it hangs in. Zero leaves that section
    // as the game shipped it.
    std::uintptr_t kSceneCaptureSetCameraViewRva;

    // Where GUObjectArray and the FName pool live, so core's reflection can
    // walk every live UObject and resolve its class and name. That walk is how
    // the retro game's own crosshair widget is found, rather than a mark being
    // painted over the top of it.
    ::cameraunlock::unreal::UObjectGlobalsLayout UObjectGlobals;

    // UWidget::SetRenderTranslation(FVector2D) - the engine's own way to move a
    // UMG widget off its laid-out position, which is what puts the retro
    // crosshair back under the shot. Calling the engine's setter rather than
    // writing RenderTransform.Translation is deliberate: the setter invalidates
    // the Slate widget's cached layout, and a raw write does not, so a raw write
    // moves the property and not the pixels.
    // The chain AORPlayerCharacter::UpdateCrosshairLocation walks to get the
    // aim point: character -> inventory -> equipped primary -> firing result ->
    // GetAimLocation. Walked directly for the retro game, whose own crosshair
    // code never runs, so a head lean there can be given the depth the shot
    // actually stops at rather than a distance this mod invented.
    std::size_t    kCharacterInventoryOffset;
    std::uintptr_t kItemSlotTagPrimaryRva;
    std::uintptr_t kGetFirstEquippedItemRva;
    std::uintptr_t kFireableStaticClassRva;
    std::uintptr_t kItemTagPrimaryFireModeRva;
    std::uintptr_t kGetFiringResultRva;

    std::uintptr_t kWidgetSetRenderTranslationRva;

    // UWidget::GetCachedGeometry, returning a const FGeometry& whose Scale at
    // +0x08 is the widget's own local-to-screen factor.
    //
    // A render translation is in the widget's LOCAL space, and Slate's layout
    // transform scales that to screen pixels. The viewport's DPI scale is NOT
    // that factor: a HUD authored at one design resolution and shown at another
    // carries the ratio too, and the retro HUD is pixel art with a design size
    // of its own. Asking the widget what its own scale actually is removes the
    // guess - it is the number Slate will use, whatever produced it.
    std::uintptr_t kWidgetGetCachedGeometryRva;
    std::size_t    kGeometryScaleOffset;

    // UWidgetLayoutLibrary::GetViewportWidgetGeometry(FGeometry& out, UObject*).
    //
    // The viewport measured in the SAME space the crosshair is measured in.
    // Everything else this mod could reach - the backbuffer, the DPI scale, the
    // HUD's own size - is in some other space, and converting between them is
    // where every version of this reticle went wrong. Asking Slate for the
    // viewport in Slate's units removes the conversion instead of getting it
    // right.
    std::uintptr_t kWidgetLayoutGetViewportGeometryRva;

    // USceneCaptureComponent2D's own projection state, read off the component
    // the SetCameraView hook is already holding.
    //
    // The mark's position is a projection, and a projection derived from a FOV
    // is only right if the engine built its matrix from that FOV. UE lets a
    // capture carry a matrix of its own instead - bUseCustomProjectionMatrix -
    // and then FOVAngle is decoration and every FOV-based formula is silently
    // wrong by whatever ratio the two disagree by. So the terms are read rather
    // than modelled: M[0][0] and M[1][1] of the matrix the capture will actually
    // render with.
    std::size_t kCaptureFovAngleOffset;
    std::size_t kCaptureProjectionTypeOffset;
    std::size_t kCaptureUseCustomProjectionOffset;
    std::size_t kCaptureCustomProjectionOffset;
};

struct BuildProfile {
    const char*   Name;
    PeFingerprint Fingerprint;
    OffsetTable   Offsets;
};

}  // namespace hol_ht
