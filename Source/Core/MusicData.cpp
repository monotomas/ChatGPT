// SPDX-License-Identifier: AGPL-3.0-or-later
// Numeric lookup adapted from Buffkiller's audited fifty-type recognition.
// Two user-requested Flysnapper extensions are appended separately below;
// the original Buffkiller reference data is not changed.
#include "MusicData.h"
#include "Reference/ChordNames.h"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

namespace flysnapper {
namespace {
constexpr uint16_t bits(std::initializer_list<int> notes) {
    uint16_t m=0; for(auto n:notes) m=uint16_t(m|(1u<<n)); return m;
}
constexpr std::array<ScaleDefinition,scaleCount> scales{{
 // Keep the original nineteen indices stable for old state and automation.
 {"chromatic","Chromatic",0xfff,0,1},
 {"ionian","Ionian (Major)",bits({0,2,4,5,7,9,11}),1,1},
 {"aeolian","Aeolian (Minor)",bits({0,2,3,5,7,8,10}),1,6},
 {"harmonic_minor","Harm. Min. 1: Harmonic Minor",bits({0,2,3,5,7,8,11}),4,1},
 {"melodic_minor","Melodic Minor 1",bits({0,2,3,5,7,9,11}),2,1},
 {"dorian","Dorian",bits({0,2,3,5,7,9,10}),1,2},
 {"phrygian","Phrygian",bits({0,1,3,5,7,8,10}),1,3},
 {"lydian","Lydian",bits({0,2,4,6,7,9,11}),1,4},
 {"mixolydian","Mixolydian",bits({0,2,4,5,7,9,10}),1,5},
 {"locrian","Locrian",bits({0,1,3,5,6,8,10}),1,7},
 {"whole_tone","Whole Tone",bits({0,2,4,6,8,10}),6,1},
 {"major_pentatonic","Major Pentatonic",bits({0,2,4,7,9}),7,1},
 {"minor_pentatonic","Minor Pentatonic",bits({0,3,5,7,10}),8,1},
 {"diminished_half_whole","Half Whole Diminished",bits({0,1,3,4,6,7,9,10}),5,1},
 {"diminished_whole_half","Whole Half Diminished",bits({0,2,3,5,6,8,9,11}),5,2},
 {"lydian_dominant","Mel. Min. 4: Lydian Dominant",bits({0,2,4,6,7,9,10}),2,4},
 {"altered","Mel. Min. 7: Altered (Super Locrian)",bits({0,1,3,4,6,8,10}),2,7},
 {"phrygian_dominant","Harm. Min. 5: Phrygian Dominant",bits({0,1,4,5,7,8,10}),4,5},
 {"minor_blues","Minor Blues",bits({0,3,5,6,7,10}),9,1},
 // New modes and bebop scales are appended; menus use metadata below.
 {"melodic_minor_2","Mel. Min. 2: Dorian ♭2 (Phrygian ♮6)",bits({0,1,3,5,7,9,10}),2,2},
 {"melodic_minor_3","Mel. Min. 3: Lydian Augmented (Lydian ♯5)",bits({0,2,4,6,8,9,11}),2,3},
 {"melodic_minor_5","Mel. Min. 5: Mixolydian ♭6 (Hindu)",bits({0,2,4,5,7,8,10}),2,5},
 {"melodic_minor_6","Mel. Min. 6: Aeolian ♭5 (Locrian ♮2 / Half Dim.)",bits({0,2,3,5,6,8,10}),2,6},
 {"harmonic_minor_2","Harm. Min. 2: Locrian ♮6",bits({0,1,3,5,6,9,10}),4,2},
 {"harmonic_minor_3","Harm. Min. 3: Ionian ♯5",bits({0,2,4,5,8,9,11}),4,3},
 {"harmonic_minor_4","Harm. Min. 4: Dorian ♯4",bits({0,2,3,6,7,9,10}),4,4},
 {"harmonic_minor_6","Harm. Min. 6: Lydian ♯2",bits({0,3,4,6,7,9,11}),4,6},
 {"harmonic_minor_7","Harm. Min. 7: Ultralocrian",bits({0,1,3,4,6,8,9}),4,7},
 {"bebop_dominant","Dominant Bebop Scale",bits({0,2,4,5,7,9,10,11}),3,1},
 {"bebop_major","Major Bebop Scale",bits({0,2,4,5,7,8,9,11}),3,2},
 {"bebop_dorian","Minor / Dorian Bebop Scale",bits({0,2,3,5,7,9,10,11}),3,3}
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
 {"minor_thirteenth","m13",5},{"thirteenth_add_sharp9","13add#9",13},{"minor_thirteenth_flat5","m13b5",9},
 // User-requested Flysnapper extensions. Lydian is a provisional compatible
 // default; these do not rename or repurpose any of the original fifty IDs.
 {"major_thirteenth_sharp11","Maj13#11",7},{"major_ninth_sharp11","Maj9#11",7}
}};
constexpr auto patterns=[] {
    std::array<buffkiller::ChordPattern,chordTypeCount> result{};
    for(size_t i=0;i<buffkiller::correctedChordPatterns.size();++i)
        result[i]=buffkiller::correctedChordPatterns[i];
    // Root/third/fifth bass policy is unchanged. Only the perfect fifth may
    // be explicitly omitted; ninth, #11, thirteenth and major seventh retain
    // the identity the user specified. Full nominal formulas drive menus.
    result[50]={"Maj13#11",bits({0,2,4,6,7,9,11}),bits({0,4,7}),0,0,bits({7})};
    result[51]={"Maj9#11",bits({0,2,4,6,7,11}),bits({0,4,7}),0,0,bits({7})};
    return result;
}();
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
        for(const auto& p:patterns) {
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
const std::array<std::vector<buffkiller::ChordCandidate>,4096>& namedLookup() {
    // GUI-only extension of the unchanged reference's named candidates.
    // Recognition on the processing thread still uses the numeric table.
    static const auto table=[] {
        auto result=buffkiller::chordLookup();
        for(int type=50;type<chordTypeCount;++type) {
            const auto& pattern=patterns[size_t(type)];
            for(uint16_t missing=pattern.omittable;;missing=uint16_t((missing-1)&pattern.omittable)) {
                const auto relative=uint16_t(pattern.intervals&~missing);
                for(int root=0;root<12;++root) {
                    buffkiller::ChordCandidate candidate;
                    candidate.name=std::string(buffkiller::chordRootNames[root])+pattern.suffix+buffkiller::omissionText(missing);
                    candidate.mask=rotateMask(relative,root);candidate.relative=relative;
                    candidate.root=root;candidate.patternIndex=type;
                    candidate.bassRoles=uint16_t(pattern.bassRoles&relative);
                    candidate.omitted=missing;candidate.omittedFifth=(missing&(1u<<7))!=0;
                    candidate.rank=100*buffkiller::chordBitCount(missing);
                    result[size_t(candidate.mask)].push_back(std::move(candidate));
                }
                if(missing==0)break;
            }
        }
        return result;
    }();
    return table;
}
std::array<std::string,12> extensionSpelling(const buffkiller::ChordCandidate& candidate) {
    std::array<std::string,12> names{};
    for(int pc=0;pc<12;++pc)names[size_t(pc)]=buffkiller::chordRootNames[pc];
    constexpr int degree[]{0,-1,1,-1,2,-1,3,4,-1,5,-1,6};
    constexpr int natural[]{0,2,4,5,7,9,11};
    const std::string letters="CDEFGAB";
    const int rootLetter=int(letters.find(buffkiller::chordRootNames[candidate.root][0]));
    for(int pc=0;pc<12;++pc)if(candidate.mask&(1u<<pc)) {
        const int offset=degree[(pc-candidate.root+12)%12];
        if(offset<0)continue;
        const int letter=(rootLetter+offset)%7;
        int accidental=(pc-natural[letter]+12)%12;if(accidental>6)accidental-=12;
        if(std::abs(accidental)<=1)
            names[size_t(pc)]=std::string(1,letters[size_t(letter)])+(accidental>0?"#":accidental<0?"b":"");
    }
    return names;
}
buffkiller::ChordDescription describeChord(uint16_t mask,int bass) {
    const auto input=buffkiller::chordInputFromMask(mask,bass);
    auto reference=buffkiller::describeChord(input);
    if(input.mask==0)return reference;
    auto result=reference;result.candidates.clear();int best=100000;
    for(auto candidate:namedLookup()[size_t(input.mask)]) {
        const int interval=(input.bassClass()-candidate.root+12)%12;
        if(!(candidate.bassRoles&(1u<<interval)))continue;
        if(candidate.rank<best){best=candidate.rank;result.candidates.clear();}
        if(candidate.rank==best) {
            candidate.rank+=interval==0?0:((interval==3||interval==4)?1:2);
            result.candidates.push_back(std::move(candidate));
        }
    }
    // Preserve the reference's exact formatting for sets whose best names
    // are unchanged, including all fallback/unsupported-bass descriptions.
    if(std::none_of(result.candidates.begin(),result.candidates.end(),[](const auto& candidate){return candidate.patternIndex>=50;}))
        return reference;
    std::stable_sort(result.candidates.begin(),result.candidates.end(),[](const auto& a,const auto& b){return a.rank<b.rank;});
    result.fallback=false;result.shortName.clear();result.detail.clear();
    const auto& first=result.candidates.front();
    result.noteNames=first.patternIndex<50?buffkiller::chordSpelling(first):extensionSpelling(first);
    result.spelledNotes=buffkiller::soundingNoteList(input,result.noteNames);
    for(size_t i=0;i<result.candidates.size();++i) {
        if(i<2){if(i)result.shortName+=" | ";result.shortName+=result.candidates[i].name;}
        if(i)result.detail+=" | ";
        result.detail+=result.candidates[i].name;
    }
    if(result.candidates.size()>2)result.shortName+=" (+"+std::to_string(result.candidates.size()-2)+")";
    result.detail="Recognised chord definitions: "+result.detail+". Notes: "+result.spelledNotes+
        ". Bass: "+result.noteNames[size_t(input.bassClass())]+
        ". Bass must be root, third or fifth of each candidate. Named omissions are explicit; no incoming tones or absent roots are hidden. Note spelling follows the first candidate. This field does not change the MIDI.";
    return result;
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
uint16_t chordToneMask(int typeIndex) noexcept {
    if(typeIndex<0||typeIndex>=chordTypeCount)return 0;
    return patterns[size_t(typeIndex)].intervals;
}
bool scaleContainsChord(int scaleIndex,uint16_t chordMask) noexcept {
    if(scaleIndex<0||scaleIndex>=scaleCount)return false;
    chordMask&=0xfff;
    return (scales[size_t(scaleIndex)].mask&chordMask)==chordMask;
}
std::array<int,scaleCount> orderedScaleIndices(uint16_t chordMask) noexcept {
    std::array<int,scaleCount> result{};
    for(int i=0;i<scaleCount;++i)result[size_t(i)]=i;
    std::sort(result.begin(),result.end(),[chordMask](int a,int b) noexcept {
        const bool compatibleA=scaleContainsChord(a,chordMask);
        const bool compatibleB=scaleContainsChord(b,chordMask);
        if(compatibleA!=compatibleB)return compatibleA;
        const auto& first=scales[size_t(a)];const auto& second=scales[size_t(b)];
        if(first.familyOrder!=second.familyOrder)return first.familyOrder<second.familyOrder;
        if(first.modeOrder!=second.modeOrder)return first.modeOrder<second.modeOrder;
        return a<b;
    });
    return result;
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
std::string chordName(uint16_t mask,int bass){return describeChord(mask,bass).shortName;}
std::string chordDetail(uint16_t mask,int bass){return describeChord(mask,bass).detail;}
}
