// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "build_profile.h"

// Steam Win64 build of High On Life (Oregon-Win64-Shipping.exe, Unreal Engine 4,
// project name "Oregon"), at Oregon\Binaries\Win64.
//
// Every number below was read out of the PDB the game ships next to that exe
// (Oregon-Win64-Shipping.pdb, full private symbols): function RVAs via dbghelp
// SymEnumSymbols, field offsets via SymGetTypeInfo, call-site return addresses
// by disassembling the named function. Nothing here is a pattern-scan guess,
// which is why there is no "candidate" language anywhere in this file.
//
// To add support for a new Steam build: do NOT edit kSteamProfile_<date> in
// place. Append a new `extern const BuildProfile kSteamProfile_YYYYMMDD = {...}`
// below, register it at the TOP of kKnownProfiles in build_registry.cpp, and
// keep older profiles forever - the PE fingerprint routes each user to theirs.

namespace hol_ht::builds {

extern const BuildProfile kSteamProfile_20231025;

// ---- Steam Win64 build (PE TimeDateStamp 0x6538811B, 2023-10-25) ----
const BuildProfile kSteamProfile_20231025 = {
    /* Name        */ "steam-win64-20231025",
    /* Fingerprint */ { 0x6538811Bu, 0x06B5E000u, 0x06497CB2u },
    /* Offsets     */ {
        // APlayerController::GetPlayerViewPoint. Virtually dispatched
        // through vtable slot 228 (+0x720), so hooking the body catches
        // every caller and the return address tells them apart.
        /* kGetPlayerViewPointRva */ 0x036329F0ULL,

        // The call is at 0x0348EE39 inside ULocalPlayer::GetViewPoint
        // (0x0348EB80); this is the instruction after it.
        /* kViewPointCallerRva    */ 0x0348EE3FULL,

        // The consumers that are drawing. Each is the instruction after a
        // call, in the function that asked for the view:
        /* kPresentationCallerRvas */ {{
            // ULocalPlayer::CalcSceneViewInitOptions + 0x69, after its
            // LocalPlayer->GetProjectionData call at 0x03488303. This is
            // the one that builds the matrices the frame is rendered with.
            0x03488309ULL,
            // ULocalPlayer::CalcSceneView + 0x20A, after its direct
            // GetViewPoint call at 0x03487D64 - the FMinimalViewInfo the
            // post-process settings and the FOV come from.
            0x03487D6AULL,
            // SWorldWidgetScreenLayer::Tick + 0x142, after its
            // GetProjectionData call - every UMG world-space widget.
            0x02F18262ULL,
            // AORPlayerCharacter::UpdateCrosshairLocation + 0x11C, after
            // ProjectWorldLocationToScreen. THE RETICLE: the game projects
            // its own weapon aim point here and writes the result to
            // AORPlayerCharacter::CrosshairLocation, so letting this one
            // through is what puts the crosshair under the shot in the
            // head-tracked view, using the engine's own matrices.
            0x017ED87CULL,
            // AORPlayerCameraManager::PushDamageIndicators + 0x3B6.
            0x017E0E96ULL,
            // USQInventoryStatics::ApplyHitResult + 0x583 - hit markers.
            0x00934F63ULL,
            // UORWidget_HUDPrompt::UpdateLocation + 0x281, after its
            // UORGameplayStatics::ProjectWorldToScreenBidirectional call at
            // 0x018351AC. That helper reaches the view through
            // ULocalPlayer::GetProjectionData directly rather than through
            // APlayerController::ProjectWorldLocationToScreen, which is why it
            // is a separate entry and not covered by any of the above.
            //
            // UpdateLocation is vtable slot 0x4C0 and UORWidget_InfoPanel
            // inherits it - checked in the image, both vtables hold 0x01834F30
            // there - so this one entry is every world-anchored HUD mark the
            // game places: the info-scan ping's panels, mission waypoints,
            // interaction prompts, dialogue speaker marks and compass markers.
            // UORWidget_HUDMaster::NativeTick calls it on each of them every
            // frame. Left off the list they projected through the clean view
            // and sat welded to the screen while the world turned behind them.
            0x018351B1ULL,
            0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL,
        }},
        // APlayerController::execProjectWorldLocationToScreen (return RVA
        // 0x039686AD) is deliberately absent, and it is the one exclusion
        // that is not about a named function. exec* is the UFunction native
        // thunk: EVERY Blueprint call of the node returns to that single
        // address, so listing it would admit an unbounded, unenumerated set
        // of callers under one entry - the opposite of what an allow-list
        // is for, and this game drives its HUD from Blueprint. Any BP node
        // that uses the projection to DECIDE something rather than to draw
        // it would become head-coupled, silently. The cost of leaving it
        // out is the one the gate says it prefers: a Blueprint-placed world
        // mark projects through the clean view and drifts under head
        // movement, which is cosmetic and visible.
        //
        // Deliberately absent for the same reason, these being the native
        // gameplay decisions that reach the same view query:
        //   0x015F0B4D AOREncounterManager::GetConditionalSpawnPoint
        //   0x017000BD AORInteractionStationManager::EnableFocus
        //   0x01780CF3 / 0x01780D5D ASQInventoryItem_Chainsaw::OnLOSTraceAllComplete
        //   0x017DA64B UORPlayerGazeComponent::PaddedLineOfSightCheck

        // Deepest listed consumer is five frames up (GetViewPoint,
        // GetProjectionData, UGameplayStatics::ProjectWorldToScreen,
        // APlayerController::ProjectWorldLocationToScreen, the caller).
        /* kPresentationStackFrames */ 8u,

        /* kGFrameCounterRva */ 0x05FAD678ULL,

        // Read out of the generated reflection setter stub rather than
        // guessed: Z_Construct_UClass_APlayerController_Statics::
        // NewProp_bShowMouseCursor_SetBit (0x03960350) is one instruction,
        // `or dword ptr [rcx+0x470], 1`, and the next stub along sets bit 2
        // of the same dword for bEnableClickEvents - the cross-check that
        // this is the bitfield and not a coincidence.
        /* kShowMouseCursorOffset */ 0x470,
        /* kShowMouseCursorMask   */ 0x1u,

        // AORPlayerController's own flags, both `bool` (one byte) in the
        // PDB's layout for the class, which is 0x700 bytes.
        /* kPauseMenuActiveOffset     */ 0x653,
        /* kCinematicModeActiveOffset */ 0x652,

        // Primary vtables of the classes those two offsets belong to.
        // AORMainMenu_PlayerController (0x04CD3B98) is deliberately absent:
        // it is the main menu's controller and never gameplay.
        /* kGameplayControllerVtableRvas */ {{
            0x04BCE790ULL,  // AORPlayerController
            0x04D5B360ULL,  // AORXSPlayerController
            0x0ULL, 0x0ULL,
        }},

        // FMinimalViewInfo: Location +0x00, Rotation +0x0C, FOV +0x18.
        // The stride is 0x0C and not 0x18 because this is UE4 - FVector and
        // FRotator are three floats here, not the three doubles UE 5.0+
        // ships with Large World Coordinates on.
        /* MinimalViewInfoLayout */ {
            /* kFovOffset      */ 0x18,
            /* kRotationStride */ 0x0C,
        },

        // AController::Character (the ACharacter this controller drives) and
        // AORCharacter::ADSOn, the game's own aiming-down-sights bool. Both
        // read out of the PDB's layout for those classes; ADSOn is the byte
        // Z_Construct_UClass_AORCharacter_Statics::NewProp_ADSOn_SetBit
        // (0x0187EFB0) writes, `mov byte ptr [rcx+0x976], 1`, and the byte
        // AORCharacter::SetADSState (0x016AAAF0) compares and stores.
        /* kCharacterOffset */ 0x280,
        /* kAdsOnOffset     */ 0x976,

        // Primary vtable of AORPlayerCharacter, the class ADSOn was read
        // from. Blueprint subclasses reuse the native vtable, so the
        // player's own BP character matches this too.
        /* kPlayerCharacterVtableRvas */ {{
            0x04BCF5C8ULL,  // AORPlayerCharacter
            0x0ULL, 0x0ULL, 0x0ULL,
        }},

        // USQFiringResultComponent::GetAimLocation, and the return address
        // of the single call to it at 0x017ED82D inside
        // AORPlayerCharacter::UpdateCrosshairLocation. The `xor r8d, r8d`
        // at 0x017ED816 is the argument this mod overrides: it clears
        // bUseAimCorrections, and with the flag clear the firing result
        // returns aimStart + direction * range instead of tracing. The same
        // flag is set (`mov r8b, 1`) at 0x0177374A in
        // UORFiringResult_Projectile::GetPlayerSpawnTransform, which is
        // where the player's own shot gets its aim point from.
        /* kGetAimLocationRva     */ 0x0094D990ULL,
        /* kCrosshairAimCallerRva */ 0x017ED832ULL,

        // USceneCaptureComponent2D::SetCameraView. The scan of every E8/E9
        // in .text finds one call to it in the whole image, at 0x0181B0D8
        // inside AORXSPlayerCharacter::UpdateSceneCapture (0x0181ADC0), so
        // the hook reaches the retro game's camera and nothing else and
        // needs no caller gate of its own. That function is reached from
        // AORXSPlayerController::UpdateCameraManager, which runs only while
        // an XS controller is driving an AORXSPlayerCharacter.
        /* kSceneCaptureSetCameraViewRva */ 0x032BB0F0ULL,

        // GUObjectArray (0x0603A2A0) + FUObjectArray::ObjObjects (0x10),
        // so this points at the FChunkedFixedUObjectArray itself, which is
        // what core's walker indexes. NumElements sits at 0x14 within it,
        // FUObjectItem is 0x18 bytes, and the chunk size is read out of
        // FChunkedFixedUObjectArray::AddRange (0x01C64160) rather than
        // assumed: it shifts the index right by 0x10 and allocates
        // 0x180008 bytes a chunk, which is 0x18 x 0x10000 plus a header.
        //
        // NamePoolData is the FNamePool, whose FNameEntryAllocator starts
        // at its offset zero with Blocks[] at 0x10. The entry header is
        // UE4.25's three bitfields in two bytes - bIsWide, a probe hash,
        // then Len - which is the layout core's ResolveFName decodes.
        /* UObjectGlobals */ {
            /* kObjObjects       */ 0x0603A2B0ULL,
            /* kObjObjects_Num   */ 0x14,
            /* kFUObjectItemSize */ 0x18,
            /* kChunkNumElems    */ 0x10000,
            /* kFNamePool        */ 0x05FFDB80ULL,
            /* kFNamePoolBlocks  */ 0x10,
            /* kClassPrivate     */ 0x10,
            /* kNamePrivate      */ 0x18,
            /* kOuterPrivate     */ 0x20,
        },

        // Read straight off AORPlayerCharacter::UpdateCrosshairLocation
        // (0x017ED760), which walks exactly this chain every frame in the
        // main game: inventory at +0xAC8, then the four calls at 0x017ED793,
        // 0x017ED79E, 0x017ED7DC and 0x017ED7E7, with the fireable class
        // check at 0x017ED7AF in between.
        /* kCharacterInventoryOffset  */ 0xAC8,
        /* kItemSlotTagPrimaryRva     */ 0x0170CF80ULL,
        /* kGetFirstEquippedItemRva   */ 0x0173DC20ULL,
        /* kFireableStaticClassRva    */ 0x0189BA80ULL,
        /* kItemTagPrimaryFireModeRva */ 0x00941CB0ULL,
        /* kGetFiringResultRva        */ 0x0093CD20ULL,

        // Two instructions: it writes the FVector2D into
        // RenderTransform.Translation at +0x90 and tail-calls
        // UWidget::UpdateRenderTransform, which is the half a raw field
        // write would miss.
        /* kWidgetSetRenderTranslationRva */ 0x02F0A470ULL,

        // FGeometry is Size, Scale, AbsolutePosition, Position,
        // AccumulatedRenderTransform - so Scale sits at +0x08.
        /* kWidgetGetCachedGeometryRva */ 0x02EC5C90ULL,
        /* kGeometryScaleOffset        */ 0x08,

        // Returns FGeometry by hidden pointer: rcx is the out-parameter and
        // rdx the world-context object.
        /* kWidgetLayoutGetViewportGeometryRva */ 0x02ECDD40ULL,

        // USceneCaptureComponent2D, from the PDB's layout for the class:
        // ProjectionType 0x2F0, FOVAngle 0x2F4,
        // bUseCustomProjectionMatrix 0xB1C, CustomProjectionMatrix 0xB20.
        // FMatrix is row-major 4x4 floats, so M[0][0] is at +0x00 of that
        // and M[1][1] at +0x14.
        /* kCaptureFovAngleOffset           */ 0x2F4,
        /* kCaptureProjectionTypeOffset     */ 0x2F0,
        /* kCaptureUseCustomProjectionOffset*/ 0xB1C,
        /* kCaptureCustomProjectionOffset   */ 0xB20,
    },
};

}  // namespace hol_ht::builds
