// SPDX-License-Identifier: AGPL-3.0-or-later
// Numeric lookup adapted from Buffkiller's audited ChordNames.h recognition.
#include "MusicData.h"
#include "Reference/ChordNames.h"
#include <algorithm>
#include <limits>

namespace flysnapper {
namespace {
constexpr uint16_t bits(std::initializer_list<int> notes) {
    uint16_t m=0; for(auto n:notes) m=uint16_t(m|(1u<<n)); return m;
}
constexpr std::array<ScaleDefinition,scaleCount> scales{{
 {"chromatic","Chromatic",0xfff},
 {"ionian","Major / Ionian",bits({0,2,4,5,7,9,11})},
 {"aeolian","Natural Minor / Aeolian",bits({0,2,3,5,7,8,10})},
 {"harmonic_minor","Harmonic Minor",bits({0,2,3,5,7,8,11})},
 {"melodic_minor","Melodic Minor",bits({0,2,3,5,7,9,11})},
 {"dorian","Dorian",bits({0,2,3,5,7,9,10})},
 {"phrygian","Phrygian",bits({0,1,3,5,7,8,10})},
 {"lydian","Lydian",bits({0,2,4,6,7,9,11})},
 {"mixolydian","Mixolydian",bits({0,2,4,5,7,9,10})},
 {"locrian","Locrian",bits({0,1,3,5,6,8,10})},
 {"whole_tone","Whole Tone",bits({0,2,4,6,8,10})},
 {"major_pentatonic","Major Pentatonic",bits({0,2,4,7,9})},
 {"minor_pentatonic","Minor Pentatonic",bits({0,3,5,7,10})},
 {"diminished_half_whole","Diminished Half-Whole",bits({0,1,3,4,6,7,9,10})},
 {"diminished_whole_half","Diminished Whole-Half",bits({0,2,3,5,6,8,9,11})},
 {"lydian_dominant","Lydian Dominant",bits({0,2,4,6,7,9,10})},
 {"altered","Altered",bits({0,1,3,4,6,8,10})},
 {"phrygian_dominant","Phrygian Dominant",bits({0,1,4,5,7,8,10})},
 {"minor_blues","Minor Blues",bits({0,3,5,6,7,10})}
}};
constexpr std::array<ChordType,chordTypeCount> types{{
 {"major","",1},{"minor","m",2},{"sus2","sus2",1},{"sus4","sus4",1},
 {"power","5",11},{"power_seventh","5/7",8},{"major_sixth","6",1},{"minor_sixth","m6",5},
 {"six_nine","6/9",1},{"dominant_seventh","7",8},{"major_seventh","Maj7",1},{"minor_seventh","m7",5},
 {"minor_major_seventh","mMaj7",4},{"half_diminished","m7b5",9},{"diminished_seventh","dim7",14},
 {"dominant_sus4","7sus4",8},{"dominant_sus4_flat9","7sus4b9",17},
 {"dominant_flat5","7b5",15},{"dominant_sharp5","7#5",16},
 {"dominant_flat9","7b9",17},{"dominant_sharp9","7#9",16},{"dominant_sharp11","7#11",15},
 {"dominant_flat13","7b13",16},{"dominant_flat5_flat9","7b5b9",16},
 {"dominant_flat5_sharp9","7b5#9",16},{"dominant_sharp5_flat9","7#5b9",16},
 {"dominant_sharp5_sharp9","7#5#9",16},{"major_seventh_sharp5","Maj7#5",4},
 {"major_seventh_sharp9","Maj7#9",3},{"major_seventh_sharp11","Maj7#11",7},
 {"dominant_ninth","9",8},{"major_ninth","Maj9",1},{"minor_ninth","m9",5},
 {"minor_major_ninth","mMaj9",4},{"ninth_sus4","9sus4",8},{"ninth_sharp11","9#11",15},
 {"dominant_eleventh","11",8},{"major_eleventh","Maj11",1},{"minor_eleventh","m11",5},
 {"minor_eleventh_flat5","m11b5",9},{"dominant_thirteenth","13",8},
 {"major_thirteenth","Maj13",1},{"minor_major_thirteenth","mMaj13",4},
 {"thirteenth_flat9","13b9",13},{"thirteenth_sharp9","13#9",13},
 {"thirteenth_sus4","13sus4",8},{"thirteenth_sus4_flat9","13sus4b9",17},
 {"minor_thirteenth","m13",5},{"thirteenth_add_sharp9","13add#9",13},{"minor_thirteenth_flat5","m13b5",9}
}};
struct Entry { ChordMatch match; int primary=100000, secondary=100000; };
using Lookup=std::array<Entry,4096*12>;
const Lookup& lookup() {
    // Keep the large table in static storage, including while initialising.
    // A local return-value table could exceed Windows' default 1 MiB stack in
    // unoptimised builds. The one-time guard is completed by Engine's ctor.
    static Lookup table{};
    static const bool initialised=[] {
        auto& result=table;
        int type=0;
        for(const auto& p:buffkiller::correctedChordPatterns) {
            const auto removable=uint16_t(p.omittable|p.optional);
            for(uint16_t missing=removable;;missing=uint16_t((missing-1)&removable)) {
                const auto relative=uint16_t(p.intervals&~missing);
                if(buffkiller::chordBitCount(relative)>=3||type==4) {
                    for(int extra=0;extra<(p.additional?2:1);++extra) {
                        const auto withExtra=uint16_t(relative|(extra?p.additional:0));
                        const int primary=100*buffkiller::chordBitCount(uint16_t(missing&~p.optional))+4*extra;
                        for(int root=0;root<12;++root) {
                            const auto mask=rotateMask(withExtra,root);
                            for(int interval=0;interval<12;++interval) {
                                if(!(p.bassRoles&withExtra&(1u<<interval))) continue;
                                const int bass=(root+interval)%12;
                                auto& entry=result[size_t(mask)*12+size_t(bass)];
                                const int secondary=primary+(interval==0?0:(interval==3||interval==4)?1:2);
                                if(primary<entry.primary) {
                                    entry.primary=primary;entry.secondary=secondary;
                                    entry.match={root,type,1};
                                } else if(primary==entry.primary) {
                                    // Every retained generated variant has a distinct
                                    // name after optional omissions are normalised.
                                    ++entry.match.candidateCount;
                                    if(secondary<entry.secondary) {
                                        entry.secondary=secondary;entry.match.root=root;entry.match.typeIndex=type;
                                    }
                                }
                            }
                        }
                    }
                }
                if(missing==0) break;
            }
            ++type;
        }
        return true;
    }();
    (void)initialised;
    return table;
}
}
const std::array<ScaleDefinition,scaleCount>& scaleDefinitions() noexcept {return scales;}
const std::array<ChordType,chordTypeCount>& chordTypes() noexcept {return types;}
std::array<int,chordTypeCount> factoryDefaults() noexcept {
    std::array<int,chordTypeCount> a{};for(size_t i=0;i<a.size();++i)a[i]=types[i].defaultScale;return a;
}
int scaleIndexFromId(const std::string& id) noexcept {
    for(int i=0;i<scaleCount;++i)if(id==scales[size_t(i)].id)return i;
    return -1;
}
int chordIndexFromId(const std::string& id) noexcept {
    for(int i=0;i<chordTypeCount;++i)if(id==types[size_t(i)].id)return i;
    return -1;
}
uint16_t rotateMask(uint16_t mask,int root) noexcept {
    root=(root%12+12)%12;mask&=0xfff;
    return uint16_t(((uint32_t(mask)<<root)|(uint32_t(mask)>>(12-root)))&0xfff);
}
void initialiseMusicData(){(void)lookup();}
ChordMatch recogniseChord(uint16_t mask,int bass) noexcept {
    if(bass<0||bass>=12||!(mask&(1u<<bass)))return {};
    return lookup()[size_t(mask&0xfff)*12+size_t(bass)].match;
}
std::string chordName(uint16_t mask,int bass){return buffkiller::describeChord(mask,bass).shortName;}
std::string chordDetail(uint16_t mask,int bass){return buffkiller::describeChord(mask,bass).detail;}
}
