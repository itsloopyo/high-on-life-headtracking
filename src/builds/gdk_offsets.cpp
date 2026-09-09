// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "build_profile.h"

// Game Pass / Microsoft Store build of High On Life
// (Oregon-WinGDK-Shipping.exe, under Oregon\Binaries\WinGDK), package version
// 1.13.3652.0. Same game, same engine, and a sibling link of the same source as
// the Steam build three hours earlier the same day - but a different target, so
// every RVA differs and the image is 2.4 MB smaller.
//
// How these numbers were obtained, because it is not the Steam route and the
// next patch has to take the same one. The WinGDK executable cannot be read
// from disk at all: gameflt.sys carries a Process Trust ACE that denies
// open-for-read to every caller, elevated or not, while every other file in the
// same directory reads normally. So there is no file to load into a
// disassembler and no PDB either - the Steam build ships full private symbols,
// this one ships nothing.
//
// The image is readable through the running process, so each address here was
// transferred from the Steam profile: the function or call site is anchored on
// what the Steam PDB names it, turned into a byte signature with only rel32
// branch targets and rip-relative displacements wildcarded, and located in a
// dump of the live module. A transfer was accepted only where that signature
// matched exactly once.
//
// Struct field offsets are offsets into a UObject rather than into the image,
// and are unchanged. That is not assumed from the builds being siblings: field
// offsets survive into the compared bytes (only addresses were wildcarded), so
// walking the two builds' copies of a function in lockstep proves the offsets
// inside it. Every offset below was confirmed that way, against the function
// named beside it.
//
// To add support for a new Game Pass build: do NOT edit this profile in place.
// Append a new kGdkProfile_YYYYMMDD below, register it at the TOP of
// kKnownProfiles in build_registry.cpp, and keep older profiles forever - the
// PE fingerprint routes each user to theirs.

namespace hol_ht::builds {

extern const BuildProfile kGdkProfile_20231025;

// ---- Game Pass WinGDK build (PE TimeDateStamp 0x6538ABA1, 2023-10-25) ----
const BuildProfile kGdkProfile_20231025 = {
    /* Name        */ "gdk-wingdk-20231025",
    /* Fingerprint */ { 0x6538ABA1u, 0x068EC000u, 0x0623CEF7u },
    /* Offsets     */ {
        // APlayerController::GetPlayerViewPoint, still vtable slot 228
        // (+0x720) - the slot index is visible in the transferred
        // ULocalPlayer::GetViewPoint, which dispatches through it.
        /* kGetPlayerViewPointRva */ 0x032B6AA0ULL,

        // The call inside ULocalPlayer::GetViewPoint is at 0x03112F29; this is
        // the instruction after it.
        /* kViewPointCallerRva    */ 0x03112F2FULL,

        // The consumers that are drawing, in the same order as the Steam
        // profile so the two files diff against each other line for line.
        /* kPresentationCallerRvas */ {{
            // ULocalPlayer::CalcSceneViewInitOptions, after its
            // GetProjectionData call at 0x0310C403 - the matrices the frame is
            // rendered with.
            0x0310C409ULL,
            // ULocalPlayer::CalcSceneView, after its direct GetViewPoint call
            // at 0x0310BE64.
            0x0310BE6AULL,
            // SWorldWidgetScreenLayer::Tick - every UMG world-space widget.
            0x02B93032ULL,
            // AORPlayerCharacter::UpdateCrosshairLocation, after
            // ProjectWorldLocationToScreen. THE RETICLE.
            0x015ED6ECULL,
            // AORPlayerCameraManager::PushDamageIndicators.
            0x015E0D06ULL,
            // USQInventoryStatics::ApplyHitResult - hit markers.
            0x008F5DA3ULL,
            // UORWidget_HUDPrompt::UpdateLocation, after its
            // UORGameplayStatics::ProjectWorldToScreenBidirectional call at
            // 0x01634FFC. Covers every world-anchored HUD mark the game
            // places, for the same vtable-sharing reason as on Steam.
            0x01635001ULL,
            0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL,
        }},
        // APlayerController::execProjectWorldLocationToScreen and the native
        // gameplay consumers are deliberately absent here too - see the Steam
        // profile for why an exec thunk must never be allow-listed.

        /* kPresentationStackFrames */ 8u,

        /* kGFrameCounterRva */ 0x05D6CFF8ULL,

        // Confirmed against APlayerController::UpdateCameraManager, which
        // tests this bitfield: 200 instructions walked in lockstep with the
        // Steam build, offset and mask unchanged.
        /* kShowMouseCursorOffset */ 0x470,
        /* kShowMouseCursorMask   */ 0x1u,

        // Confirmed against AORPlayerController::SetCinematicMode and
        // AORPlayerController::execSetPauseMenuActiveState respectively.
        /* kPauseMenuActiveOffset     */ 0x653,
        /* kCinematicModeActiveOffset */ 0x652,

        // AORXSPlayerController overrides exactly one slot of
        // AORPlayerController's vtable in this game - slot 345, PostProcess -
        // so that slot is the only thing separating the two, and it is what
        // identified each of these.
        /* kGameplayControllerVtableRvas */ {{
            0x04894498ULL,  // AORPlayerController
            0x04A21920ULL,  // AORXSPlayerController
            0x0ULL, 0x0ULL,
        }},

        // Still UE4's float FVector/FRotator, so Rotation is 0x0C past
        // Location rather than 0x18. Confirmed in the transferred
        // ULocalPlayer::GetViewPoint.
        /* MinimalViewInfoLayout */ {
            /* kFovOffset      */ 0x18,
            /* kRotationStride */ 0x0C,
        },

        // Confirmed against AController::SetPawn and APlayerController::
        // BeginPlay (Character) and AORCharacter::SetADSState (ADSOn).
        /* kCharacterOffset */ 0x280,
        /* kAdsOnOffset     */ 0x976,

        /* kPlayerCharacterVtableRvas */ {{
            0x048952D0ULL,  // AORPlayerCharacter
            0x0ULL, 0x0ULL, 0x0ULL,
        }},

        // USQFiringResultComponent::GetAimLocation, and the return address of
        // the call to it inside AORPlayerCharacter::UpdateCrosshairLocation at
        // 0x015ED69D. Overriding bUseAimCorrections at that one call site is
        // what puts the crosshair on the impact point instead of the ray end.
        /* kGetAimLocationRva     */ 0x0090E7E0ULL,
        /* kCrosshairAimCallerRva */ 0x015ED6A2ULL,

        // USceneCaptureComponent2D::SetCameraView, the retro game's camera.
        /* kSceneCaptureSetCameraViewRva */ 0x02F3E9E0ULL,

        // The two addresses move; everything else about the layout is a
        // property of UE 4.25 and is unchanged.
        /* UObjectGlobals */ {
            /* kObjObjects       */ 0x05DF9C10ULL,
            /* kObjObjects_Num   */ 0x14,
            /* kFUObjectItemSize */ 0x18,
            /* kChunkNumElems    */ 0x10000,
            /* kFNamePool        */ 0x05DBD500ULL,
            /* kFNamePoolBlocks  */ 0x10,
            /* kClassPrivate     */ 0x10,
            /* kNamePrivate      */ 0x18,
            /* kOuterPrivate     */ 0x20,
        },

        // The inventory offset is confirmed in the transferred
        // UpdateCrosshairLocation, which loads it every frame.
        //
        // The two gameplay-tag accessors could not be matched on their own
        // prologues: they are compiler-generated magic-static wrappers, and
        // every one of them in the image is identical apart from a TLS slot
        // constant that differs between the two builds. They were taken from
        // the calls to them inside UpdateCrosshairLocation instead.
        /* kCharacterInventoryOffset  */ 0xAC8,
        /* kItemSlotTagPrimaryRva     */ 0x0150CC60ULL,
        /* kGetFirstEquippedItemRva   */ 0x0153DA80ULL,
        /* kFireableStaticClassRva    */ 0x0169B980ULL,
        /* kItemTagPrimaryFireModeRva */ 0x00902B00ULL,
        /* kGetFiringResultRva        */ 0x008FDB70ULL,

        /* kWidgetSetRenderTranslationRva */ 0x02B85320ULL,

        /* kWidgetGetCachedGeometryRva */ 0x02B41840ULL,
        /* kGeometryScaleOffset        */ 0x08,

        /* kWidgetLayoutGetViewportGeometryRva */ 0x02B48EE0ULL,

        // ProjectionType and FOVAngle are confirmed in the transferred
        // SetCameraView; bUseCustomProjectionMatrix and CustomProjectionMatrix
        // against USceneCaptureComponent2D's constructor and
        // FScene::UpdateSceneCaptureContents, which is the code that consumes
        // them.
        /* kCaptureFovAngleOffset           */ 0x2F4,
        /* kCaptureProjectionTypeOffset     */ 0x2F0,
        /* kCaptureUseCustomProjectionOffset*/ 0xB1C,
        /* kCaptureCustomProjectionOffset   */ 0xB20,
    },
};

}  // namespace hol_ht::builds
