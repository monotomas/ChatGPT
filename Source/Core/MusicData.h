// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace flysnapper {
constexpr int scaleCount = 38, chordTypeCount = 52;
// Array position is the persisted parameter value. Display order is separate:
// existing positions/IDs remain stable when new scale families are appended.
struct ScaleDefinition {
    const char* id;
    const char* name;
    uint16_t mask;
    int familyOrder;
    int modeOrder;
};
struct ChordType { const char* id; const char* suffix; int defaultScale; };
const std::array<ScaleDefinition, scaleCount>& scaleDefinitions() noexcept;
const std::array<ChordType, chordTypeCount>& chordTypes() noexcept;
// GUI-only label in root C. Presentation is separate from stable IDs/suffixes:
// type zero is "C Major"; invalid indices return an empty string. Allocates.
std::string chordTypeDisplayNameInC(int typeIndex);
std::array<int, chordTypeCount> factoryDefaults() noexcept;
int scaleIndexFromId(const std::string& id) noexcept;
int chordIndexFromId(const std::string& id) noexcept;
// Nominal chord-type formula relative to C, including tones recognition may
// permit omitted. An invalid type has no formula and returns zero.
uint16_t chordToneMask(int typeIndex) noexcept;
// Both masks use the low twelve pitch-class bits. A zero chord is contained
// in every valid scale; an invalid scale index is never compatible.
bool scaleContainsChord(int scaleIndex, uint16_t chordMask) noexcept;
// Compatible scales first, then requested parent-family and mode order within
// each group. Passing zero supplies the global fixed-scale menu order.
std::array<int, scaleCount> orderedScaleIndices(uint16_t chordMask=0) noexcept;
uint16_t rotateMask(uint16_t mask, int root) noexcept;
struct ChordMatch { int root=-1, typeIndex=-1, candidateCount=0; };
// Call initialiseMusicData before entering realtime processing. Engine does so
// in its constructor. Recognition subsequently accesses fixed numeric tables.
void initialiseMusicData();
ChordMatch recogniseChord(uint16_t mask, int bass) noexcept;
std::string chordName(uint16_t mask, int bass); // GUI ONLY, allocates
std::string chordDetail(uint16_t mask, int bass); // GUI ONLY, allocates
}
