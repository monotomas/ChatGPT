// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace flysnapper {
constexpr int scaleCount = 19, chordTypeCount = 50;
struct ScaleDefinition { const char* id; const char* name; uint16_t mask; };
struct ChordType { const char* id; const char* suffix; int defaultScale; };
const std::array<ScaleDefinition, scaleCount>& scaleDefinitions() noexcept;
const std::array<ChordType, chordTypeCount>& chordTypes() noexcept;
std::array<int, chordTypeCount> factoryDefaults() noexcept;
int scaleIndexFromId(const std::string& id) noexcept;
int chordIndexFromId(const std::string& id) noexcept;
uint16_t rotateMask(uint16_t mask, int root) noexcept;
struct ChordMatch { int root=-1, typeIndex=-1, candidateCount=0; };
// Call initialiseMusicData before entering realtime processing. Engine does so
// in its constructor. Recognition subsequently accesses fixed numeric tables.
void initialiseMusicData();
ChordMatch recogniseChord(uint16_t mask, int bass) noexcept;
std::string chordName(uint16_t mask, int bass); // GUI ONLY, allocates
std::string chordDetail(uint16_t mask, int bass); // GUI ONLY, allocates
}
