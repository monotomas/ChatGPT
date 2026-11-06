// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "ChordDictionary.h"
#include "ChordInput.h"
#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>
#include <utility>

namespace buffkiller {
inline constexpr uint16_t chordBits(std::initializer_list<int> notes) noexcept {
    uint16_t m=0;for(int n:notes)m=uint16_t(m|(1u<<((n%12+12)%12)));return m;
}
inline constexpr uint16_t rotateChord(uint16_t m,int root) noexcept {
    root=(root%12+12)%12;m&=0xfff;return uint16_t(((uint32_t(m)<<root)|(uint32_t(m)>>(12-root)))&0xfff);
}
inline int chordBitCount(uint16_t m)noexcept{int n=0;for(;m;m=uint16_t(m&(m-1)))++n;return n;}
inline const auto& chordPatterns()noexcept{return correctedChordPatterns;}
inline constexpr const char* chordRootNames[]{"C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
struct ChordCandidate {
    std::string name;
    uint16_t mask=0,relative=0,bassRoles=0,omitted=0;
    int root=0,patternIndex=0,rank=0;
    bool omittedFifth=false;
};
struct ChordDescription {
    std::string shortName,detail,spelledNotes;
    std::array<std::string,12> noteNames{};
    std::vector<ChordCandidate> candidates;
    bool fallback=false;
};
inline std::string omissionText(uint16_t m) {
    std::string out;
    // Only the degrees explicitly allowed by generate_chord_dictionary.py.
    for(const auto item:{std::pair<int,const char*>{7,"5"},{10,"7"},{11,"7"},{2,"9"},{5,"11"}})
        if(m&(1u<<item.first)){if(!out.empty())out+=", ";out+="no ";out+=item.second;}
    return out.empty()?out:" ("+out+")";
}
inline const std::array<std::vector<ChordCandidate>,4096>& chordLookup() {
    // Constructed once, on first GUI request, NEVER from audio processing.
    // At each MIDI change only the small list for that exact set is filtered.
    static const auto table=[] {
        std::array<std::vector<ChordCandidate>,4096> result;
        int index=0;
        for(const auto& p:correctedChordPatterns) {
            uint16_t removable=uint16_t(p.omittable|p.optional);
            for(uint16_t missing=removable;;missing=uint16_t((missing-1)&removable)) {
                const uint16_t relative=uint16_t(p.intervals&~missing);
                if(chordBitCount(relative)>=3||std::string(p.suffix)=="5") {
                    for(int extraCase=0;extraCase<(p.additional?2:1);++extraCase) {
                        const uint16_t withExtra=uint16_t(relative|(extraCase?p.additional:0));
                        const uint16_t explicitOmissions=uint16_t(missing&~p.optional);
                        for(int root=0;root<12;++root) {
                            const auto mask=rotateChord(withExtra,root);
                            ChordCandidate c;
                            c.name=std::string(chordRootNames[root])+p.suffix+omissionText(explicitOmissions);
                            c.mask=mask;c.relative=withExtra;c.root=root;c.patternIndex=index;
                            c.bassRoles=uint16_t(p.bassRoles&withExtra);c.omitted=explicitOmissions;
                            c.omittedFifth=(explicitOmissions&(1u<<7))!=0;
                            // Fewer invented omissions first, then the direct
                            // reference over a voicing with an additional ninth.
                            c.rank=100*chordBitCount(explicitOmissions)+4*extraCase;
                            auto& bucket=result[size_t(mask)];
                            const auto found=std::find_if(bucket.begin(),bucket.end(),[&](const auto& x){return x.name==c.name;});
                            if(found==bucket.end())bucket.push_back(std::move(c));
                            else if(c.rank<found->rank)*found=std::move(c);
                        }
                    }
                }
                if(missing==0)break;
            }
            ++index;
        }
        return result;
    }();
    return table;
}
inline ChordInput chordInputFromMask(uint16_t mask,int bassClass=-1) noexcept {
    ChordInput in;mask&=0xfff;
    if(mask==0)return in;
    if(bassClass<0||bassClass>11||!(mask&(1u<<bassClass))) {
        for(bassClass=0;bassClass<12&&!(mask&(1u<<bassClass));++bassClass){}
    }
    in.lowestMidi=bassClass;in.mask=mask;
    for(int step=0;step<12;++step){int c=(bassClass+step)%12;if(mask&(1u<<c))in.ordered[size_t(in.count++)]=c;}
    return in;
}
inline std::string soundingNoteList(const ChordInput& in) {
    std::string result;uint16_t seen=0;
    for(int i=0;i<in.count&&i<12;++i){const int c=in.ordered[size_t(i)];if(c<0||c>11||(seen&(1u<<c)))continue;
        if(!result.empty())result+=", ";result+=chordRootNames[c];seen=uint16_t(seen|(1u<<c));}
    for(int c=0;c<12;++c)if((in.mask&(1u<<c))&&!(seen&(1u<<c))){if(!result.empty())result+=", ";result+=chordRootNames[c];}
    return result.empty()?"\xe2\x80\x94":result;
}
// Roles determine the readable spelling AFTER numeric recognition. MIDI has
// no spelling/key signature; never modify the input pitch-class set.
inline std::array<std::string,12> chordSpelling(const ChordCandidate& c) {
    std::array<std::string,12> result;
    for(int pc=0;pc<12;++pc)result[size_t(pc)]=chordRootNames[pc];
    const std::string root=chordRootNames[c.root],suffix=correctedChordPatterns[size_t(c.patternIndex)].suffix;
    const std::string letters="CDEFGAB";
    constexpr int natural[]{0,2,4,5,7,9,11};
    const int rootLetter=int(letters.find(root[0]));
    const bool minor=suffix.rfind("m",0)==0,dim=suffix.rfind("dim",0)==0;
    constexpr const char* sharps[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    constexpr const char* flats[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    const bool preferSharp=root.find('b')==std::string::npos && c.root!=5;
    for(int pc=0;pc<12;++pc)if(c.mask&(1u<<pc)) {
        const int interval=(pc-c.root+12)%12;
        int degree=1;
        switch(interval) {
            case 0:degree=1;break;case 1:case 2:degree=2;break;
            case 3:degree=(minor||dim)?3:2;break;case 4:degree=3;break;
            case 5:degree=4;break;
            case 6:degree=(dim||suffix.find("b5")!=std::string::npos)?5:4;break;
            case 7:degree=5;break;
            case 8:degree=suffix.find("#5")!=std::string::npos?5:6;break;
            case 9:degree=dim?7:6;break;case 10:case 11:degree=7;break;
        }
        const int li=(rootLetter+degree-1)%7;
        int accidental=(pc-natural[li]+12)%12;if(accidental>6)accidental-=12;
        if(std::abs(accidental)<=1)result[size_t(pc)]=std::string(1,letters[size_t(li)])+(accidental==1?"#":accidental==-1?"b":"");
        else result[size_t(pc)]=preferSharp?sharps[pc]:flats[pc]; // readable G, not F##, in E13add#9
    }
    return result;
}
inline std::string soundingNoteList(const ChordInput& in,const std::array<std::string,12>& names) {
    std::string out;uint16_t seen=0;
    auto append=[&](int c){if(c<0||c>11||(seen&(1u<<c)))return;
        if(!out.empty())out+=", ";out+=names[size_t(c)];seen=uint16_t(seen|(1u<<c));};
    for(int i=0;i<in.count&&i<12;++i)append(in.ordered[size_t(i)]);
    for(int c=0;c<12;++c)if(in.mask&(1u<<c))append(c);
    return out.empty()?"\xe2\x80\x94":out;
}
inline ChordDescription describeChord(const ChordInput& in) {
    ChordDescription result;const uint16_t mask=in.mask&0xfff;
    result.shortName=soundingNoteList(in);result.fallback=true;
    for(int pc=0;pc<12;++pc)result.noteNames[size_t(pc)]=chordRootNames[pc];
    result.spelledNotes=result.shortName;
    const int bass=in.bassClass();
    if(mask!=0&&bass>=0&&bass<12&&(mask&(1u<<bass))) {
        int best=100000;
        for(auto c:chordLookup()[size_t(mask)]) {
            const int interval=(bass-c.root+12)%12;
            // Absolute rule, NOT a ranking penalty: no 7th/9th/11th/13th bass.
            if(!(c.bassRoles&(1u<<interval)))continue;
            // Keep all equally complete interpretations, but don't flood the
            // field with omitted-tone guesses when an exact label exists.
            if(c.rank<best){best=c.rank;result.candidates.clear();}
            if(c.rank==best){c.rank+=interval==0?0:((interval==3||interval==4)?1:2);result.candidates.push_back(std::move(c));}
        }
        std::stable_sort(result.candidates.begin(),result.candidates.end(),[](const auto& a,const auto& b){return a.rank<b.rank;});
    }
    if(result.candidates.empty()) {
        result.detail=mask?"Sounding notes, ordered by their lowest active MIDI occurrence: "+result.shortName+
            ". No supported name satisfies the tone set and root/third/fifth bass rule. MIDI contains no enharmonic spelling; a consistent mixed sharp/flat spelling is used. Audio is unchanged.":"No active MIDI notes.";
        return result;
    }
    result.fallback=false;result.shortName.clear();
    result.noteNames=chordSpelling(result.candidates.front());
    result.spelledNotes=soundingNoteList(in,result.noteNames);
    for(size_t i=0;i<result.candidates.size();++i) {
        if(i<2){if(i)result.shortName+=" | ";result.shortName+=result.candidates[i].name;}
        if(i)result.detail+=" | ";result.detail+=result.candidates[i].name;
    }
    if(result.candidates.size()>2)result.shortName+=" (+"+std::to_string(result.candidates.size()-2)+")";
    result.detail="Audited chord definitions: "+result.detail+". Notes: "+result.spelledNotes+". Bass: "+result.noteNames[size_t(bass)]+
        ". Bass must be root, third or fifth of each candidate. Named omissions are explicit; no incoming tones or absent roots are hidden. Reference 13 chords include natural 11 unless #11 is specified; natural 11 may be omitted in supported thirteenth voicings. Note spelling follows the first candidate, with readable single accidentals. This field does not change the sound.";
    return result;
}
inline ChordDescription describeChord(uint16_t mask,int bassClass=-1){return describeChord(chordInputFromMask(mask,bassClass));}
} // namespace buffkiller
