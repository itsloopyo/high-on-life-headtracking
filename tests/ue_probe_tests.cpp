// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

// The two guarded reads the gameplay gate and the ADS gate share.
//
// Both gates decide whether head tracking runs from a byte at a build-profile
// offset, and both are wrong in the same silent way if that offset moves: a byte
// of some other property toggles several times a second, and the view stops and
// starts with nothing in the log saying why. What stops that is the pair of
// rules below - a bool is 0 or 1 and nothing else, and the class has to be one
// the profile actually took its offsets from - so they are pinned here rather
// than left to be re-read out of two call sites.

#include <array>
#include <cstdint>

#include "test_harness.h"
#include "ue_probe.h"

#include "cameraunlock/unreal/ue_runtime.h"

namespace {

namespace ue = ::cameraunlock::unreal;
using hol_ht::ue_probe::BoolRead;
using hol_ht::ue_probe::ReadEngineBool;
using hol_ht::ue_probe::VtableIsOneOf;

std::uintptr_t AddressOf(const void* p) { return reinterpret_cast<std::uintptr_t>(p); }

void ZeroAndOneAreTheOnlyBools() {
    std::uint16_t cell = 0;
    unsigned byte = 0xffu;
    CHECK(ReadEngineBool(AddressOf(&cell), byte) == BoolRead::False);
    CHECK(byte == 0u);

    cell = 1;
    CHECK(ReadEngineBool(AddressOf(&cell), byte) == BoolRead::True);
    CHECK(byte == 1u);
}

// The reason both gates latch themselves off on this: acting on a byte that is
// neither would put the view in and out of a menu at random, and the log line
// that says so needs the byte itself.
void AnythingElseIsNotABoolAndReportsTheByte() {
    std::uint16_t cell = 2;
    unsigned byte = 0;
    CHECK(ReadEngineBool(AddressOf(&cell), byte) == BoolRead::NotABool);
    CHECK(byte == 2u);

    cell = 0x00ff;
    CHECK(ReadEngineBool(AddressOf(&cell), byte) == BoolRead::NotABool);
    CHECK(byte == 0xffu);
}

// Core has no byte-wide guarded read, so this takes two bytes. The second one
// belongs to the next property and must not reach the verdict - a neighbouring
// flag going high would otherwise turn every read into NotABool and disable the
// gate for the session.
void TheNeighbouringPropertyByteIsDiscarded() {
    std::uint16_t cell = 0xff01;
    unsigned byte = 0;
    CHECK(ReadEngineBool(AddressOf(&cell), byte) == BoolRead::True);
    CHECK(byte == 1u);

    cell = 0xff00;
    CHECK(ReadEngineBool(AddressOf(&cell), byte) == BoolRead::False);
    CHECK(byte == 0u);
}

// An unreadable address answers Unreadable rather than faulting, which is what
// lets both gates read straight off an engine pointer with no validity check of
// their own.
void AnUnmappedAddressIsUnreadable() {
    unsigned byte = 0;
    CHECK(ReadEngineBool(0x10, byte) == BoolRead::Unreadable);
}

void VtableMatchingAcceptsOnlyListedClasses() {
    // A stand-in for the game module: any address inside `image` counts as an
    // in-module vtable, and its RVA is the offset from the start.
    std::array<std::uintptr_t, 4> image{};
    const std::uintptr_t base = AddressOf(image.data());
    ue::SetRuntime(base, base + sizeof(image), ue::UObjectGlobalsLayout{});

    constexpr std::uintptr_t kListedRva = sizeof(std::uintptr_t);
    const std::array<std::uintptr_t, 4> known{{ kListedRva, 0, 0, 0 }};

    std::uintptr_t listed = base + kListedRva;
    CHECK(VtableIsOneOf(AddressOf(&listed), known));

    std::uintptr_t unlisted = base + 2 * sizeof(std::uintptr_t);
    CHECK(!VtableIsOneOf(AddressOf(&unlisted), known));

    // A subclass from another module, or a wild pointer: outside the image, so
    // no RVA of it means anything against the profile.
    std::uintptr_t outside = base + sizeof(image);
    CHECK(!VtableIsOneOf(AddressOf(&outside), known));

    std::uintptr_t none = 0;
    CHECK(!VtableIsOneOf(AddressOf(&none), known));

    CHECK(!VtableIsOneOf(0x10, known));
}

// Trailing zeroes are the unused slots of a fixed-size profile array. A zero RVA
// is the start of the module, so matching one would make every unused slot claim
// the image base.
void UnusedTrailingSlotsNeverMatch() {
    std::array<std::uintptr_t, 4> image{};
    const std::uintptr_t base = AddressOf(image.data());
    ue::SetRuntime(base, base + sizeof(image), ue::UObjectGlobalsLayout{});

    const std::array<std::uintptr_t, 4> known{{ 0, 0, 0, 0 }};
    std::uintptr_t atBase = base;
    CHECK(!VtableIsOneOf(AddressOf(&atBase), known));
}

}  // namespace

int main() {
    ZeroAndOneAreTheOnlyBools();
    AnythingElseIsNotABoolAndReportsTheByte();
    TheNeighbouringPropertyByteIsDiscarded();
    AnUnmappedAddressIsUnreadable();
    VtableMatchingAcceptsOnlyListedClasses();
    UnusedTrailingSlotsNeverMatch();

    return hol_test::Report();
}
