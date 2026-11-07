// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Core/MidiEngine.h"
#include "Core/Reference/ChordNames.h"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <set>
#include <sstream>
#include <string>

namespace {bool countAllocations=false;size_t allocationCount=0;size_t assertions=0;}
void* operator new(std::size_t n) {
    if(countAllocations)++allocationCount;
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void* operator new(std::size_t n,const std::nothrow_t&) noexcept {
    try{return ::operator new(n);}catch(...){return nullptr;}
}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept {
    return ::operator new(n,std::nothrow);
}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
void operator delete(void* p,const std::nothrow_t&) noexcept{std::free(p);}
void operator delete[](void* p,const std::nothrow_t&) noexcept{std::free(p);}

namespace {
using namespace flysnapper;
#define CHECK(x) do{++assertions;if(!(x)){std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" " #x <<"\n";std::exit(1);}}while(false)
void major(Engine& engine) {auto s=engine.settings();s.mode=Mode::FixedScale;s.root=0;s.scale=1;engine.setSettings(s);}
void on(Engine& e,int n,Output& out,int t=0,Source src=Source::External,int channel=1) {
    e.beginTimestamp(src,channel,1);e.noteOn(src,channel,n,100,t,out);
}
void off(Engine& e,int n,Output& out,int t=0,Source src=Source::External,int channel=1){e.noteOff(src,channel,n,0,t,out);}
void chord(Engine& e,std::initializer_list<int> notes){e.clearChord();for(auto n:notes)e.chordNoteOn(n);e.refreshChord();}
void scaleCatalogAndMenus() {
    static_assert(scaleCount<=63,"Packed snapshots reserve six bits for scale index plus one");
    constexpr std::array<const char*,31> legacyIds{{
        "chromatic","ionian","aeolian","harmonic_minor","melodic_minor",
        "dorian","phrygian","lydian","mixolydian","locrian","whole_tone",
        "major_pentatonic","minor_pentatonic","diminished_half_whole",
        "diminished_whole_half","lydian_dominant","altered","phrygian_dominant","minor_blues",
        "melodic_minor_2","melodic_minor_3","melodic_minor_5","melodic_minor_6",
        "harmonic_minor_2","harmonic_minor_3","harmonic_minor_4","harmonic_minor_6",
        "harmonic_minor_7","bebop_dominant","bebop_major","bebop_dorian"
    }};
    // These are the original parameter values, not positions in a new menu.
    for(size_t i=0;i<legacyIds.size();++i)CHECK(scaleIndexFromId(legacyIds[i])==int(i));
    // User's corrected52-row worksheet and explicit chromatic sharp9-13 assignments,
    // independently mapped by stable IDs.
    // These replace old provisional choices. Both sharp9-thirteenth formulas
    // use Chromatic as explicitly requested, with all52 formulas contained.
    constexpr std::array<int,52> userFactoryDefaults{{
        1,2,8,5,12,5,1,4,1,8,7,5,4,22,14,5,19,15,16,17,13,15,21,16,16,16,
        16,20,26,7,15,7,5,3,8,15,8,1,5,22,8,1,4,35,0,8,35,5,0,32,7,7
    }};
    for(size_t i=0;i<userFactoryDefaults.size();++i)CHECK(factoryDefaults()[i]==userFactoryDefaults[i]);
    CHECK(chordIndexFromId("dominant_sharp9")==20);
    CHECK(chordIndexFromId("dominant_flat13")==22);
    CHECK(chordIndexFromId("thirteenth_sharp9")==44);
    CHECK(chordIndexFromId("thirteenth_add_sharp9")==48);
    CHECK(chordIndexFromId("major_thirteenth_sharp11")==50);
    CHECK(chordIndexFromId("major_ninth_sharp11")==51);
    const auto& definitions=scaleDefinitions();
    const auto checkModes=[&](int family,const std::array<int,7>& parent){
        std::array<bool,7> found{};int count=0;
        for(const auto& definition:definitions)if(definition.familyOrder==family){
            CHECK(definition.modeOrder>=1&&definition.modeOrder<=7);
            const size_t mode=size_t(definition.modeOrder-1);CHECK(!found[mode]);found[mode]=true;++count;
            uint16_t expected=0;for(const int tone:parent)expected=uint16_t(expected|(1u<<((tone-parent[mode]+12)%12)));
            CHECK(definition.mask==expected);CHECK(buffkiller::chordBitCount(definition.mask)==7);
        }
        CHECK(count==7);for(bool present:found)CHECK(present);
    };
    checkModes(1,{{0,2,4,5,7,9,11}});
    checkModes(2,{{0,2,3,5,7,9,11}}); // Jazz melodic minor, same both directions.
    checkModes(4,{{0,2,3,5,7,8,11}});
    checkModes(5,{{0,2,4,5,7,8,11}});
    const auto exact=[&](const char* id,std::initializer_list<int> tones){
        const int index=scaleIndexFromId(id);CHECK(index>=0);
        CHECK(definitions[size_t(index)].mask==buffkiller::chordBits(tones));
    };
    exact("chromatic",{0,1,2,3,4,5,6,7,8,9,10,11});
    exact("bebop_dominant",{0,2,4,5,7,9,10,11});
    exact("bebop_major",{0,2,4,5,7,8,9,11});
    exact("bebop_dorian",{0,2,3,5,7,9,10,11});
    exact("diminished_half_whole",{0,1,3,4,6,7,9,10});
    exact("diminished_whole_half",{0,2,3,5,6,8,9,11});
    exact("whole_tone",{0,2,4,6,8,10});
    exact("major_pentatonic",{0,2,4,7,9});
    exact("minor_pentatonic",{0,3,5,7,10});
    exact("minor_blues",{0,3,5,6,7,10});
    exact("harmonic_major",{0,2,4,5,7,8,11});
    exact("harmonic_major_mode2",{0,2,3,5,6,9,10});
    exact("harmonic_major_mode3",{0,1,3,4,7,8,10});
    exact("harmonic_major_mode4",{0,2,3,6,7,9,11});
    exact("harmonic_major_mode5",{0,1,4,5,7,9,10});
    exact("harmonic_major_mode6",{0,3,4,6,8,9,11});
    exact("harmonic_major_mode7",{0,1,3,5,6,8,9});
    constexpr std::array<const char*,7> melodicNames{{
        "Melodic Minor 1","Mel. Min. 2: Dorian ♭2 (Phrygian ♮6)",
        "Mel. Min. 3: Lydian Augmented (Lydian ♯5)","Mel. Min. 4: Lydian Dominant",
        "Mel. Min. 5: Mixolydian ♭6 (Hindu)",
        "Mel. Min. 6: Aeolian ♭5 (Locrian ♮2 / Half Dim.)",
        "Mel. Min. 7: Altered (Super Locrian)"
    }};
    for(const auto& definition:definitions){
        CHECK(definition.mask>0&&definition.mask<=0xfff&&(definition.mask&1));
        CHECK(definition.familyOrder>=0&&definition.familyOrder<=10);
        if(definition.familyOrder==2)CHECK(std::string(definition.name)==melodicNames[size_t(definition.modeOrder-1)]);
    }
    // The requested display contract is explicit and independent of persisted
    // array order. This also checks ordering between scale parent families.
    constexpr std::array<const char*,7> harmonicMajorNames{{
        "Harmonic Major 1","Harmonic Major 2 (Dorian b5)",
        "Harmonic Major 3 (Phrygian b4)","Harmonic Major 4 (Lydian b3)",
        "Harmonic Major 5 (Mixolydian b2)","Harmonic Major 6 (Lydian Augmented #2)",
        "Harmonic Major 7 (Locrian bb7)"
    }};
    for(int mode=0;mode<7;++mode){
        const auto& definition=definitions[size_t(31+mode)];
        CHECK(definition.familyOrder==5&&definition.modeOrder==mode+1);
        CHECK(std::string(definition.name)==harmonicMajorNames[size_t(mode)]);
        CHECK(scaleIndexFromId(mode==0?"harmonic_major":"harmonic_major_mode"+std::to_string(mode+1))==31+mode);
    }
    constexpr std::array<const char*,38> globalIds{{
        "chromatic","ionian","dorian","phrygian","lydian","mixolydian","aeolian","locrian",
        "melodic_minor","melodic_minor_2","melodic_minor_3","lydian_dominant",
        "melodic_minor_5","melodic_minor_6","altered",
        "bebop_dominant","bebop_major","bebop_dorian",
        "harmonic_minor","harmonic_minor_2","harmonic_minor_3","harmonic_minor_4",
        "phrygian_dominant","harmonic_minor_6","harmonic_minor_7",
        "harmonic_major","harmonic_major_mode2","harmonic_major_mode3","harmonic_major_mode4",
        "harmonic_major_mode5","harmonic_major_mode6","harmonic_major_mode7",
        "diminished_half_whole","diminished_whole_half","whole_tone",
        "major_pentatonic","minor_pentatonic","minor_blues"
    }};
    const auto global=orderedScaleIndices();
    for(size_t i=0;i<global.size();++i)CHECK(std::string(definitions[size_t(global[i])].id)==globalIds[i]);
    CHECK(chordToneMask(-1)==0&&chordToneMask(chordTypeCount)==0);
    CHECK(chordToneMask(50)==buffkiller::chordBits({0,2,4,6,7,9,11}));
    CHECK(chordToneMask(51)==buffkiller::chordBits({0,2,4,6,7,11}));
    CHECK(chordToneMask(44)==buffkiller::chordBits({0,3,4,5,7,9,10}));
    CHECK(chordToneMask(48)==buffkiller::chordBits({0,2,3,4,5,7,9,10}));
    for(int scale=0;scale<scaleCount;++scale) {
        // Both retained sharp9 formulas need the chromatic catalog option
        // for complete containment. User assignments remain selectable.
        CHECK(scaleContainsChord(scale,chordToneMask(44))==(scale==0));
        CHECK(scaleContainsChord(scale,chordToneMask(48))==(scale==0));
    }
    CHECK(!scaleContainsChord(-1,0)&&!scaleContainsChord(scaleCount,0));
    for(int type=0;type<chordTypeCount;++type){
        const auto mask=chordToneMask(type);
        if(type<50)CHECK(mask==buffkiller::correctedChordPatterns[size_t(type)].intervals);
        CHECK(scaleContainsChord(factoryDefaults()[size_t(type)],mask));
        const auto actual=orderedScaleIndices(mask);std::array<int,scaleCount> expected{};size_t at=0;
        // Stable filtering of the independently specified parent/mode order
        // must produce both selectable groups, not remove incompatible scales.
        for(bool compatible:{true,false})for(const char* id:globalIds){
            const int index=scaleIndexFromId(id);CHECK(index>=0);
            const bool contains=(definitions[size_t(index)].mask&mask)==mask;
            if(contains==compatible)expected[at++]=index;
        }
        CHECK(at==size_t(scaleCount)&&actual==expected);
        std::array<bool,scaleCount> seen{};
        for(int index:actual){CHECK(index>=0&&index<scaleCount);CHECK(!seen[size_t(index)]);seen[size_t(index)]=true;}
        for(bool present:seen)CHECK(present);
        for(int index=0;index<scaleCount;++index)for(int root=0;root<12;++root){
            const auto chordRotated=rotateMask(mask,root),scaleRotated=rotateMask(definitions[size_t(index)].mask,root);
            CHECK(scaleContainsChord(index,mask)==((chordRotated&scaleRotated)==chordRotated));
        }
    }
    // A C-major chord's compatible church modes precede the other modes,
    // preserving mode numbers separately in both selectable groups.
    std::array<std::string,7> majorModes{};size_t at=0;
    for(int index:orderedScaleIndices(buffkiller::chordBits({0,4,7})))
        if(definitions[size_t(index)].familyOrder==1)majorModes[at++]=definitions[size_t(index)].id;
    CHECK(at==7);
    CHECK((majorModes==std::array<std::string,7>{{"ionian","lydian","mixolydian","dorian","phrygian","aeolian","locrian"}}));
    // The two bebop octatonic scales differ in exactly the specified added
    // passing tone; no scale is silently aliased to another catalog entry.
    CHECK(scaleIndexFromId("bebop_dominant")!=scaleIndexFromId("bebop_major"));
}
void chordTonePresentation() {
    // Literal musical-role examples distinguish root position from either
    // absolute C-first ordering or a simple root-rotated pitch-class sort.
    const auto display=[](const char* id,const char* expected) {
        const int type=chordIndexFromId(id);CHECK(type>=0);
        CHECK(chordToneNamesInRootPosition(chordToneMask(type),0,type)==expected);
    };
    display("major","C  E  G");
    display("minor","C  Eb  G");
    display("sus2","C  D  G");
    display("sus4","C  F  G");
    display("power","C  G");
    display("power_seventh","C  G  Bb");
    display("major_sixth","C  E  G  A");
    display("minor_sixth","C  Eb  G  A");
    display("six_nine","C  E  G  A  D");
    display("dominant_flat5","C  E  Gb  Bb");
    display("dominant_sharp5","C  E  G#  Bb");
    display("dominant_flat9","C  E  G  Bb  Db");
    display("dominant_sharp9","C  E  G  Bb  D#");
    display("dominant_sharp11","C  E  G  Bb  F#");
    display("dominant_flat13","C  E  G  Bb  Ab");
    display("dominant_sharp5_sharp9","C  E  G#  Bb  D#");
    display("dominant_ninth","C  E  G  Bb  D");
    display("ninth_sus4","C  F  G  Bb  D");
    display("dominant_eleventh","C  E  G  Bb  D  F");
    display("minor_eleventh_flat5","C  Eb  Gb  Bb  D  F");
    display("dominant_thirteenth","C  E  G  Bb  D  F  A");
    display("major_thirteenth","C  E  G  B  D  F  A");
    display("minor_major_thirteenth","C  Eb  G  B  D  F  A");
    display("thirteenth_sharp9","C  E  G  Bb  D#  F  A");
    display("thirteenth_add_sharp9","C  E  G  Bb  D  D#  F  A");
    display("thirteenth_sus4","C  F  G  Bb  D  A");
    display("minor_thirteenth_flat5","C  Eb  Gb  Bb  D  F  A");
    display("major_thirteenth_sharp11","C  E  G  B  D  F#  A");
    display("major_ninth_sharp11","C  E  G  B  D  F#");
    // Retain the existing readable spelling for diminished seventh (A is
    // enharmonic Bbb); it still occupies the seventh role, not a sixth/13th.
    display("diminished_seventh","C  Eb  Gb  A");

    constexpr std::array<const char*,12> majorNames{{
        "C  E  G","Db  F  Ab","D  F#  A","Eb  G  Bb","E  G#  B","F  A  C",
        "F#  A#  C#","G  B  D","Ab  C  Eb","A  C#  E","Bb  D  F","B  D#  F#"
    }};
    for(int root=0;root<12;++root) {
        const auto mask=rotateMask(buffkiller::chordBits({0,4,7}),root);
        // The actual recognized root, rather than the played bass, controls
        // the order in all three inversions, including G/B and Eb/G.
        for(int degree:{0,4,7}) {
            const auto match=recogniseChord(mask,(root+degree)%12);
            CHECK(match.root==root&&match.typeIndex==0);
            CHECK(chordToneNamesInRootPosition(mask,match.root,match.typeIndex)==majorNames[size_t(root)]);
        }
    }
    const int sharpEleventh=chordIndexFromId("dominant_sharp11");
    CHECK(chordToneNamesInRootPosition(buffkiller::chordBits({0,2,4,6,7,10}),0,sharpEleventh)=="C  E  G  Bb  D  F#");
    const int majorSharpEleventh=chordIndexFromId("major_seventh_sharp11");
    CHECK(chordToneNamesInRootPosition(buffkiller::chordBits({0,2,4,6,7,11}),0,majorSharpEleventh)=="C  E  G  B  D  F#");
    // Actual sounding tones remain authoritative: never fill an omitted
    // fifth from the nominal formula or discard a recognized added ninth.
    const int lydian=chordIndexFromId("major_thirteenth_sharp11");
    CHECK(chordToneNamesInRootPosition(buffkiller::chordBits({0,2,4,6,9,11}),0,lydian)=="C  E  B  D  F#  A");
    CHECK(chordToneNamesInRootPosition(buffkiller::chordBits({2,4,6,9,11}),0,lydian)=="D  E  F#  A  B");

    const auto pitchClass=[](const std::string& token) {
        const std::string letters="CDEFGAB";constexpr int naturals[]{0,2,4,5,7,9,11};
        const auto at=letters.find(token[0]);CHECK(at!=std::string::npos);
        int pitch=naturals[at];
        for(size_t i=1;i<token.size();++i){CHECK(token[i]=='#'||token[i]=='b');pitch+=token[i]=='#'?1:-1;}
        return (pitch%12+12)%12;
    };
    // Every full catalog formula in every root must preserve each received
    // pitch class exactly once and lead with the root. This invariant does
    // not duplicate the implementation's harmonic-role sorting algorithm.
    for(int type=0;type<chordTypeCount;++type)for(int root=0;root<12;++root) {
        const auto mask=rotateMask(chordToneMask(type),root);
        std::istringstream text(chordToneNamesInRootPosition(mask,root,type));
        std::string token;uint16_t seen=0;int count=0;
        while(text>>token) {
            const int pc=pitchClass(token);
            if(count==0)CHECK(pc==root);
            CHECK(!(seen&(1u<<pc)));seen=uint16_t(seen|(1u<<pc));++count;
        }
        CHECK(seen==mask&&count==buffkiller::chordBitCount(mask));
    }
    CHECK(chordToneNamesInRootPosition(0,-1,-1)=="none");
    CHECK(chordToneNamesInRootPosition(0,0,0)=="none");
    CHECK(chordToneNamesInRootPosition(uint16_t(1u<<7),-1,-1)=="G");
    CHECK(chordToneNamesInRootPosition(buffkiller::chordBits({0,1,7}),-1,-1)=="C  Db  G");
    CHECK(chordToneNamesInRootPosition(buffkiller::chordBits({2,7,11}),7,-1)=="D  G  B");
    CHECK(chordToneNamesInRootPosition(buffkiller::chordBits({2,7,11}),12,0)=="D  G  B");
    CHECK(chordToneNamesInRootPosition(uint16_t(0x8000|(1u<<7)),-1,-1)=="G");
}
void dataAndRecognition() {
    scaleCatalogAndMenus();
    CHECK(scaleDefinitions().size()==38);CHECK(chordTypes().size()==52);
    CHECK(buffkiller::correctedChordPatterns.size()==50); // Reference provenance is unchanged.
    std::set<std::string> ids;
    for(int i=0;i<scaleCount;++i){CHECK(ids.insert(scaleDefinitions()[size_t(i)].id).second);CHECK(scaleIndexFromId(scaleDefinitions()[size_t(i)].id)==i);}
    ids.clear();
    for(int i=0;i<chordTypeCount;++i){CHECK(ids.insert(chordTypes()[size_t(i)].id).second);CHECK(chordIndexFromId(chordTypes()[size_t(i)].id)==i);CHECK(factoryDefaults()[size_t(i)]>=0&&factoryDefaults()[size_t(i)]<scaleCount);}
    CHECK(scaleIndexFromId("missing")==-1);CHECK(chordIndexFromId("missing")==-1);
    CHECK(scaleDefinitions()[0].mask==0xfff);
    for(const auto& s:scaleDefinitions())for(int root=0;root<12;++root) {
        auto rotated=rotateMask(s.mask,root);
        CHECK(buffkiller::chordBitCount(rotated)==buffkiller::chordBitCount(s.mask));
        for(int pc=0;pc<12;++pc)CHECK(bool(rotated&(1u<<((pc+root)%12)))==bool(s.mask&(1u<<pc)));
    }
    initialiseMusicData();
    chordTonePresentation();
    // Exhaustive parity with the unchanged fifty-type reference plus an
    // independent oracle for the two requested formulas and explicit no5.
    // No masks are skipped merely because an extension changes the result.
    size_t extendedSets=0;
    for(int mask=0;mask<4096;++mask)for(int bass=0;bass<12;++bass) {
        const auto ref=buffkiller::describeChord(uint16_t(mask),bass);
        if(!(mask&(1u<<bass)))continue; // reference picks a replacement bass
        const auto match=recogniseChord(uint16_t(mask),bass);
        ChordMatch expected{};int primary=100000,secondary=100000;
        const auto bassPenalty=[&](int root){const int interval=(bass-root+12)%12;return interval==0?0:(interval==3||interval==4)?1:2;};
        if(!ref.candidates.empty()) {
            const auto& first=ref.candidates.front();primary=first.rank-bassPenalty(first.root);secondary=first.rank;
            expected={first.root,first.patternIndex,int(ref.candidates.size())};
        }
        for(int type=50;type<52;++type)for(bool omittedFifth:{false,true})for(int root=0;root<12;++root) {
            const auto formula=type==50?buffkiller::chordBits({0,2,4,6,7,9,11}):buffkiller::chordBits({0,2,4,6,7,11});
            const auto voiced=omittedFifth?uint16_t(formula&~(1u<<7)):formula;
            if(rotateMask(voiced,root)!=mask)continue;
            const int interval=(bass-root+12)%12;
            const bool validBass=interval==0||interval==4||(!omittedFifth&&interval==7);
            if(!validBass)continue;
            const int candidatePrimary=omittedFifth?100:0,candidateSecondary=candidatePrimary+bassPenalty(root);
            if(candidatePrimary<primary){primary=candidatePrimary;secondary=candidateSecondary;expected={root,type,1};}
            else if(candidatePrimary==primary){++expected.candidateCount;if(candidateSecondary<secondary){secondary=candidateSecondary;expected.root=root;expected.typeIndex=type;}}
        }
        CHECK(match.candidateCount==expected.candidateCount);
        CHECK(match.typeIndex==expected.typeIndex);CHECK(match.root==expected.root);
        if(expected.typeIndex>=50||expected.candidateCount!=int(ref.candidates.size()))++extendedSets;
    }
    CHECK(extendedSets>0);
    // Presentation explicitly distinguishes a lone pitch class from a major
    // triad without changing numeric recognition, suffixes or stable IDs.
    CHECK(std::string(chordTypes()[0].suffix).empty());
    CHECK(chordTypeDisplayNameInC(-1).empty()&&chordTypeDisplayNameInC(chordTypeCount).empty());
    for(int type=0;type<chordTypeCount;++type)
        CHECK(chordTypeDisplayNameInC(type)==(type==0?"C Major":std::string("C")+chordTypes()[size_t(type)].suffix));
    for(int root=0;root<12;++root) {
        const auto single=uint16_t(1u<<root);
        const auto singleLabel=std::string(buffkiller::chordRootNames[root])+" (single note)";
        CHECK(recogniseChord(single,root).typeIndex==-1);
        CHECK(chordName(single,root)==singleLabel);
        CHECK(chordDetail(single,root).find(singleLabel)!=std::string::npos);
        const auto majorMask=rotateMask(buffkiller::chordBits({0,4,7}),root);
        const auto majorLabel=std::string(buffkiller::chordRootNames[root])+" Major";
        for(int degree:{0,4,7}) {
            const int bass=(root+degree)%12;
            CHECK(recogniseChord(majorMask,bass).typeIndex==0);
            CHECK(chordName(majorMask,bass)==majorLabel);
            CHECK(chordDetail(majorMask,bass).find(majorLabel)!=std::string::npos);
        }
    }
    CHECK(chordName(buffkiller::chordBits({0,7}),0)=="C5");
    CHECK(chordName(buffkiller::chordBits({0,7,10}),0)=="C5/7");
    CHECK(chordName(buffkiller::chordBits({0,4,7,11}),0).find("CMaj7")!=std::string::npos);
    const auto regression=[](std::initializer_list<int> pcs,int bass,const char* name){
        CHECK(chordName(buffkiller::chordBits(pcs),bass).find(name)!=std::string::npos);
    };
    regression({11,4,7,2,6,9,1},11,"Em13");
    regression({11,4,7,2,6,9,1,8},11,"E13add#9");
    regression({11,2,6,9,1,4,8,3},11,"B13add#9");
    regression({6,0,4,9,3,8},6,"F#m13b5");
    // Full Lydian pitch classes also describe Am13 with its third in bass.
    // The exact root-C extension wins the existing bass ranking, while the
    // equally complete alternative remains visible rather than disappearing.
    const auto lydian=recogniseChord(buffkiller::chordBits({0,2,4,6,7,9,11}),0);
    CHECK(lydian.typeIndex==50&&lydian.root==0&&lydian.candidateCount>=2);
    const auto label=chordName(buffkiller::chordBits({0,2,4,6,7,9,11}),0);
    CHECK(label.rfind("CMaj13#11",0)==0&&label.find("Am13")!=std::string::npos);
    CHECK(chordDetail(buffkiller::chordBits({0,2,4,6,7,9,11}),0).find("F#")!=std::string::npos);
    // The explicit ninth label outranks the legacy Maj7#11-plus-added9 case.
    const auto ninth=recogniseChord(buffkiller::chordBits({0,2,4,6,7,11}),0);
    CHECK(ninth.typeIndex==51&&ninth.root==0);
    CHECK(chordName(buffkiller::chordBits({0,2,4,6,7,11}),0).rfind("CMaj9#11",0)==0);
    for(int type=50;type<52;++type)for(int root=0;root<12;++root) {
        const auto full=rotateMask(chordToneMask(type),root);
        for(int degree:{0,4,7}) {
            const auto named=recogniseChord(full,(root+degree)%12);
            const auto text=chordName(full,(root+degree)%12);
            const auto requestedName=std::string(buffkiller::chordRootNames[root])+chordTypes()[size_t(type)].suffix;
            if(type==50&&degree==7) {
                // C-Lydian/G is also G-major-thirteenth in root position.
                // Preserve that legitimate first interpretation; the new
                // C-major-13#11 name remains an equally complete alternative.
                CHECK(named.typeIndex==41&&named.root==(root+7)%12&&named.candidateCount==2);
                CHECK(text.find(" | "+requestedName)!=std::string::npos);
            } else {
                CHECK(named.typeIndex==type&&named.root==root);
                CHECK(text.rfind(requestedName,0)==0);
            }
        }
        const auto noFifth=rotateMask(uint16_t(chordToneMask(type)&~(1u<<7)),root);
        const auto named=recogniseChord(noFifth,root);CHECK(named.typeIndex==type&&named.root==root);
        CHECK(chordName(noFifth,root).find("(no 5)")!=std::string::npos);
        const auto packed=unpackSnapshot(packSnapshot({full,0,root,type,1,root}));
        CHECK(packed.typeIndex==type&&packed.root==root); // New IDs fit the six-bit field.
    }
    // Ninth/#11/thirteenth bass is not newly allowed for the C extensions.
    for(int degree:{2,6,9})CHECK(recogniseChord(chordToneMask(50),degree).typeIndex!=50);
}
void directionHistory() {
    Output o;
    struct Run {std::array<int,3> input;int expected;};
    constexpr std::array<Run,6> runs{{
        {{{65,67,68}},69},{{{65,64,63}},62},{{{67,65,68}},67},
        {{{67,67,66}},65},{{{67,67,68}},69},{{{62,61,63}},62}
    }};
    // The original prior-two-input table must be identical with all keys
    // overlapping and with detached playing. The last case distinguishes raw
    // descending62,61 from its two equal corrected destinations62,62.
    for(const bool legato:{false,true})for(const auto& run:runs) {
        Engine e;major(e);
        for(size_t i=0;i<run.input.size();++i) {
            o.clear();on(e,run.input[i],o,int(i)*10);
            CHECK(o.size>0&&o.events[o.size-1].type==EventType::NoteOn);
            if(i+1==run.input.size())CHECK(o.events[o.size-1].note==run.expected);
            if(!legato)off(e,run.input[i],o,int(i)*10+5);
        }
        CHECK(e.activeVoices()==(legato?3u:0u));o.clear();e.panic(40,o);
    }
    // Other streams have their own attack histories, even on the same output
    // channel. Note Off, sustain release and identical settings retain history.
    for(int action=0;action<10;++action) {
        Engine e;major(e);o.clear();on(e,67,o);on(e,65,o,10);
        switch(action) {
        case 0:on(e,60,o,11,Source::Generated);on(e,64,o,12,Source::Generated);break;
        case 1:on(e,60,o,11,Source::External,2);on(e,64,o,12,Source::External,2);break;
        case 2:e.stopSource(Source::Generated,12,o);break;
        case 3:e.controller(Source::Generated,1,123,0,12,o);break;
        case 4:e.controller(Source::External,2,120,0,12,o);break;
        case 5:off(e,67,o,11);off(e,65,o,12);break;
        case 6:e.controller(Source::External,1,64,127,11,o);off(e,67,o,12);off(e,65,o,13);e.controller(Source::External,1,64,0,14,o);break;
        case 7:e.setSettings(e.settings());break;
        case 8:e.beginTimestamp(Source::Generated,1,2);break;
        case 9:e.beginTimestamp(Source::External,2,2);break;
        }
        o.clear();on(e,68,o,20);CHECK(o.events[o.size-1].note==67);
    }
    // Actual settings changes, own-source stop, own-channel all-notes-off and
    // global panic clear history, so the next ambiguous distance goes upward.
    for(int action=0;action<4;++action) {
        Engine e;major(e);o.clear();on(e,67,o);on(e,65,o,10);
        if(action==0){auto s=e.settings();s.root=1;e.setSettings(s);s.root=0;e.setSettings(s);}
        if(action==1)e.stopSource(Source::External,12,o);
        if(action==2)e.controller(Source::External,1,123,0,12,o);
        if(action==3)e.panic(12,o);
        o.clear();on(e,68,o,20);CHECK(o.events[o.size-1].note==69);
    }
    // One stream's simultaneous group cannot use event order as a melody or
    // overwrite another source's established descending history.
    for(const bool reverse:{false,true}) {
        Engine e;major(e);o.clear();on(e,67,o);on(e,65,o,10);
        on(e,67,o,11,Source::Generated);on(e,65,o,12,Source::Generated);
        o.clear();e.beginTimestamp(Source::External,1,2);
        e.noteOn(Source::External,1,reverse?68:66,90,20,o);
        e.noteOn(Source::External,1,reverse?66:68,91,20,o);
        std::array<int,2> attacks{};size_t count=0;
        for(size_t i=0;i<o.size;++i)if(o.events[i].type==EventType::NoteOn){CHECK(count<attacks.size());attacks[count++]=o.events[i].note;}
        CHECK(count==2);std::sort(attacks.begin(),attacks.end());CHECK(attacks[0]==67&&attacks[1]==69);
        o.clear();on(e,68,o,21,Source::Generated);CHECK(o.events[o.size-1].note==67);
        o.clear();on(e,63,o,22);CHECK(o.events[o.size-1].note==64); // Group seeded neither history entry.
    }
    {
        Engine e;major(e);o.clear();on(e,69,o);on(e,67,o,10);
        e.beginTimestamp(Source::External,1,2);
        e.noteOn(Source::External,1,67,90,20,o);e.noteOn(Source::External,1,67,91,20,o);
        o.clear();on(e,66,o,30);CHECK(o.events[o.size-1].note==67); // Duplicates are conservatively ambiguous too.
    }
    // A cleanup message inside the current sample group clears pitch history,
    // but cannot turn later simultaneous attacks into a sequential melody.
    // Cover both On orders and cleanup before, between and after the attacks.
    for(int cleanup=0;cleanup<3;++cleanup)for(int position=0;position<3;++position)
        for(const bool reverse:{false,true})for(const bool descendingProbe:{false,true}) {
        Engine e;major(e);o.clear();on(e,60,o);o.clear();
        e.beginTimestamp(Source::External,1,2);
        for(int step=0;step<3;++step) {
            if(step==position) {
                if(cleanup<2)e.controller(Source::External,1,cleanup==0?120:123,0,20,o);
                else e.stopSource(Source::External,20,o);
            }
            if(step<2)e.noteOn(Source::External,1,step==0?(reverse?65:67):(reverse?67:65),90,20,o);
        }
        CHECK(e.activeVoices()==size_t(2-position));
        size_t oldNoteOffs=0;
        for(size_t i=0;i<o.size;++i)if(o.events[i].type==EventType::NoteOff&&o.events[i].note==60){++oldNoteOffs;CHECK(o.events[i].offset==20);}
        CHECK(oldNoteOffs==1);
        o.clear();on(e,descendingProbe?64:68,o,30);
        CHECK(o.events[o.size-1].note==(descendingProbe?64:69));
        if(descendingProbe) {
            // Exposes even one incorrectly seeded group note after a cleanup
            // between Ons: only64 should precede63, so fallback is upward64.
            o.clear();on(e,63,o,31);CHECK(o.events[o.size-1].note==64);
        }
        for(int note:{60,65,67})off(e,note,o,32);
        CHECK(e.activeVoices()==size_t(descendingProbe?2:1));
        off(e,descendingProbe?64:68,o,33);
        if(descendingProbe)off(e,63,o,34);
        CHECK(e.activeVoices()==0);
    }
    {
        Engine e;major(e);o.clear();on(e,67,o);on(e,65,o,10);o.clear();
        for(const auto invalid:{Source(-1),Source(3)}) {
            e.beginTimestamp(invalid,1,2);e.noteOn(invalid,1,60,100,12,o);
            e.noteOff(invalid,1,67,0,12,o);e.controller(invalid,1,64,127,12,o);e.stopSource(invalid,12,o);
        }
        CHECK(o.size==0&&e.activeVoices()==2);on(e,68,o,20);CHECK(o.events[o.size-1].note==67);
    }
}
void correction() {
    const auto cm=scaleDefinitions()[1].mask;
    CHECK(Engine::nearest(68,cm,67,65)==69);
    CHECK(Engine::nearest(63,cm,64,65)==62);
    CHECK(Engine::nearest(68,cm,65,67)==67);
    CHECK(Engine::nearest(66,cm,67,67)==65);
    CHECK(Engine::nearest(68,cm,67,67)==69);
    CHECK(Engine::nearest(68,cm)==69);
    CHECK(Engine::nearest(61,0)==-1);
    CHECK(Engine::nearest(-1,cm)==-1);CHECK(Engine::nearest(128,cm)==-1);
    // Every nonempty pitch-class set, every MIDI note: preserve allowed notes
    // and choose a globally nearest valid candidate, including both MIDI edges.
    for(int mask=1;mask<4096;++mask)for(int n=0;n<128;++n) {
        int y=Engine::nearest(n,uint16_t(mask));
        CHECK(y>=0&&y<=127);CHECK(mask&(1u<<(y%12)));
        if(mask&(1u<<(n%12)))CHECK(y==n);
        for(int d=0;d<std::abs(y-n);++d){
            CHECK(n-d<0||!(mask&(1u<<((n-d)%12))));
            CHECK(n+d>127||!(mask&(1u<<((n+d)%12))));
        }
    }
    for(int n=0;n<128;++n)CHECK(Engine::nearest(n,0xfff)==n);
    Engine e;major(e);Output o;
    for(int n:{65,67,68}){o.clear();on(e,n,o);CHECK(o.size==1);off(e,n,o);}
    CHECK(o.events[0].note==69);
    // Polyphonic timestamps use the documented upper-candidate fallback.
    o.clear();on(e,67,o);off(e,67,o);on(e,65,o);off(e,65,o);o.clear();
    e.beginTimestamp(2);e.noteOn(Source::External,1,68,90,100,o);e.noteOn(Source::External,1,70,90,100,o);
    CHECK(o.events[0].note==69);CHECK(o.events[0].offset==100);
    directionHistory();
}
void snapshotPacking() {
    // Exercise the new high scale bit alongside every other status field.
    // Existing words retain their exact original layout; unused high bits
    // remain ignored. None of this word is part of saved XML/preset state.
    for(int scale=-1;scale<scaleCount;++scale)for(int root=-1;root<12;++root)
        for(int mode=0;mode<3;++mode)for(bool enabled:{false,true}) {
            Snapshot expected{0xad5,0x8d5,root,root<0?-1:51,255,root,scale,root,mode,enabled};
            const auto packed=packSnapshot(expected);
            const auto verify=[&](const Snapshot& actual){
                CHECK(actual.chordMask==expected.chordMask&&actual.allowedMask==expected.allowedMask);
                CHECK(actual.root==expected.root&&actual.typeIndex==expected.typeIndex);
                CHECK(actual.candidateCount==expected.candidateCount&&actual.bass==expected.bass);
                CHECK(actual.effectiveScale==expected.effectiveScale&&actual.effectiveRoot==expected.effectiveRoot);
                CHECK(actual.mode==expected.mode&&actual.correctionEnabled==expected.correctionEnabled);
            };
            verify(unpackSnapshot(packed));verify(unpackSnapshot(packed|(uint64_t(31)<<59)));
            const auto legacy=uint64_t(expected.chordMask)|(uint64_t(expected.allowedMask)<<12)
                |(uint64_t(root+1)<<24)|(uint64_t(expected.typeIndex+1)<<28)
                |(uint64_t(255)<<34)|(uint64_t(root+1)<<42)
                |(uint64_t((scale+1)&31)<<46)|(uint64_t(root+1)<<51)
                |(uint64_t(mode)<<55)|(uint64_t(enabled)<<57);
            CHECK(packed==(scale<31?legacy:legacy|(uint64_t(1)<<58)));
            CHECK((packed>>59)==0);
        }
}
void unknownChordFallback() {
    Output o;
    for(const auto mode:{Mode::ChordNotes,Mode::ChordScale}) {
        Engine e;auto settings=e.settings();settings.mode=mode;settings.root=7;settings.scale=3;
        settings.chordScales[51]=18;e.setSettings(settings);
        for(int unknown=0;unknown<5;++unknown) {
            switch(unknown) {
            case 0:e.clearChord();break;
            case 1:chord(e,{55});break;
            case 2:chord(e,{43,55,67});break; // Octave duplicates remain one pitch class.
            case 3:chord(e,{60,61});break;
            case 4:chord(e,{60,61,62});break;
            }
            const auto snapshot=unpackSnapshot(packSnapshot(e.snapshot()));
            CHECK(snapshot.typeIndex==-1&&snapshot.root==-1&&snapshot.allowedMask==0xfff);
            CHECK(snapshot.effectiveScale==0&&snapshot.effectiveRoot==0&&snapshot.mode==int(mode)&&snapshot.correctionEnabled);
            CHECK(e.settings().mode==settings.mode&&e.settings().root==7&&e.settings().scale==3&&e.settings().chordScales==settings.chordScales);
            if(unknown==1||unknown==2)CHECK(formatChord(snapshot)=="G (single note)");
            for(int note=0;note<128;++note) {
                o.clear();on(e,note,o,10);CHECK(o.size==1&&o.events[0].type==EventType::NoteOn&&o.events[0].note==note);
                off(e,note,o,11);CHECK(o.size==2&&o.events[1].type==EventType::NoteOff&&o.events[1].note==note);
            }
            CHECK(e.activeVoices()==0);
        }
        // A recognised two-pitch power chord still applies correction; the
        // fallback depends on recognition, not merely a count of chord notes.
        chord(e,{48,55});CHECK(e.snapshot().typeIndex==4&&e.snapshot().root==0);
        const auto expectedMask=mode==Mode::ChordNotes?uint16_t(0x081):scaleDefinitions()[size_t(settings.chordScales[4])].mask;
        CHECK(e.snapshot().allowedMask==expectedMask);
        o.clear();on(e,61,o);CHECK(o.events[o.size-1].note==60);off(e,61,o);
        // Re-entering a recognised chord corrects new attacks while existing
        // chromatic/corrected bindings retain their original FIFO identities.
        e.clearChord();o.clear();on(e,63,o,20);CHECK(o.size==1&&o.events[0].note==63);
        chord(e,{48,52,55});on(e,63,o,21);CHECK(o.size==2&&o.events[1].note==64);
        e.clearChord();on(e,63,o,22);CHECK(o.size==4&&o.events[2].type==EventType::NoteOff&&o.events[2].note==63&&o.events[3].note==63);
        o.clear();off(e,63,o,23);CHECK(o.size==0&&e.activeVoices()==2);
        off(e,63,o,24);CHECK(o.size==1&&o.events[0].note==64&&e.activeVoices()==1);
        off(e,63,o,25);CHECK(o.size==2&&o.events[1].note==63&&e.activeVoices()==0);
        CHECK(e.settings().chordScales==settings.chordScales&&e.settings().mode==mode);
    }
    // Unknown chord input never overrides the explicit fixed-scale choice.
    Engine fixed;major(fixed);
    for(const bool single:{false,true}) {
        if(single)chord(fixed,{55});else fixed.clearChord();
        CHECK(fixed.snapshot().typeIndex==-1&&fixed.snapshot().allowedMask==scaleDefinitions()[1].mask);
        CHECK(fixed.snapshot().effectiveScale==1&&fixed.snapshot().effectiveRoot==0);
        o.clear();on(fixed,61,o);CHECK(o.events[0].note==62);off(fixed,61,o);
    }
}
void chordControlAndScales() {
    snapshotPacking();
    unknownChordFallback();
    // Real new user defaults include high indices32/35 and the fractional-
    // normalization regression's index3. Test fresh core instances without
    // overriding their maps, including the packed GUI status high bit.
    constexpr std::array<std::pair<int,int>,9> freshFactoryCases{{{43,35},{49,32},{33,3},{10,7},{31,7},{20,13},{22,21},{44,0},{48,0}}};
    for(const auto& expected:freshFactoryCases){
        Engine fresh;auto settings=fresh.settings();settings.mode=Mode::ChordScale;fresh.setSettings(settings);
        const auto formula=chordToneMask(expected.first);
        for(int pc=0;pc<12;++pc)if(formula&(1u<<pc))fresh.chordNoteOn(pc==0?36:60+pc);
        fresh.refreshChord();const auto snapshot=unpackSnapshot(packSnapshot(fresh.snapshot()));
        CHECK(snapshot.root==0&&snapshot.typeIndex==expected.first);
        CHECK(snapshot.effectiveScale==expected.second&&snapshot.allowedMask==scaleDefinitions()[size_t(expected.second)].mask);
    }
    Engine e;Output o;
    chord(e,{36,48,64,67});CHECK(e.snapshot().chordMask==0x091);CHECK(e.snapshot().root==0);CHECK(e.snapshot().typeIndex==0);
    e.chordNoteOff(36);e.refreshChord();CHECK(e.snapshot().chordMask==0x091);
    e.chordNoteOn(48);e.chordNoteOff(48);e.refreshChord();CHECK(e.snapshot().chordMask==0x091);
    e.chordNoteOff(48);e.refreshChord();CHECK(e.snapshot().chordMask==0x090);
    chord(e,{60,64,67});on(e,25,o,17);CHECK(o.events[0].note==24);CHECK(o.events[0].offset==17);
    off(e,25,o,31);CHECK(o.events[1].note==24);CHECK(o.events[1].offset==31);
    o.clear();on(e,121,o);CHECK(o.events[0].note==120);off(e,121,o);
    // Every chord type has its independent scale selection. A recognised
    // candidate (including ambiguities) uses precisely that type's rule.
    Settings s=e.settings();s.mode=Mode::ChordScale;
    for(int type=0;type<chordTypeCount;++type)for(int root=0;root<12;++root) {
        const auto mask=rotateMask(chordToneMask(type),root);
        e.clearChord();
        for(int pc=0;pc<12;++pc)if(mask&(1u<<pc))e.chordNoteOn(pc==root?36+pc:60+pc);
        e.refreshChord();const auto match=e.snapshot();CHECK(match.typeIndex>=0);
        for(int scale=0;scale<scaleCount;++scale) {
            s.chordScales[size_t(match.typeIndex)]=scale;e.setSettings(s);
            CHECK(e.snapshot().allowedMask==rotateMask(scaleDefinitions()[size_t(scale)].mask,match.root));
        }
    }
    // Bm7 and Em7 share the minor_seventh rule, inversions keep the root.
    s.chordScales=factoryDefaults();s.chordScales[11]=5;e.setSettings(s);
    chord(e,{47,54,62,69});CHECK(e.snapshot().root==11);CHECK(e.snapshot().typeIndex==11);
    CHECK(e.snapshot().allowedMask==rotateMask(scaleDefinitions()[5].mask,11));
    chord(e,{40,47,55,62});CHECK(e.snapshot().root==4);CHECK(e.snapshot().typeIndex==11);
    CHECK(e.snapshot().allowedMask==rotateMask(scaleDefinitions()[5].mask,4));
    chord(e,{44,47,52});CHECK(e.snapshot().root==4);CHECK(e.snapshot().bass==8); // E/G#
    chord(e,{40,47,55,62});
    s.chordScales[11]=0;e.setSettings(s);CHECK(e.snapshot().allowedMask==0xfff);
    CHECK(e.settings().chordScales[32]==factoryDefaults()[32]);
    Engine independent;CHECK(independent.settings().chordScales[11]==5);
    e.clearChord();CHECK(e.snapshot().allowedMask==0xfff);o.clear();on(e,60,o);CHECK(o.size==1&&o.events[0].note==60);off(e,60,o);CHECK(o.size==2&&e.activeVoices()==0);
    chord(e,{60,61});CHECK(e.snapshot().typeIndex==-1);CHECK(e.snapshot().allowedMask==0xfff);
    const auto pack=packSnapshot(e.snapshot());const auto unpack=unpackSnapshot(pack);
    CHECK(unpack.chordMask==e.snapshot().chordMask);CHECK(unpack.allowedMask==e.snapshot().allowedMask);CHECK(unpack.typeIndex==-1);
    CHECK(unpack.effectiveScale==0&&unpack.effectiveRoot==0&&unpack.mode==1&&unpack.correctionEnabled);
    s.mode=Mode::FixedScale;s.root=11;s.scale=18;e.setSettings(s);
    const auto fixed=unpackSnapshot(packSnapshot(e.snapshot()));
    CHECK(fixed.effectiveScale==18&&fixed.effectiveRoot==11&&fixed.mode==2&&fixed.correctionEnabled);
    CHECK(fixed.allowedMask==rotateMask(scaleDefinitions()[18].mask,11));
    // Every appended scale survives settings, transposition and the packed
    // GUI snapshot, including index37 which uses the new sixth scale bit.
    for(int scale=0;scale<scaleCount;++scale)for(int root=0;root<12;++root){
        s.scale=scale;s.root=root;e.setSettings(s);
        const auto expanded=unpackSnapshot(packSnapshot(e.snapshot()));
        CHECK(expanded.effectiveScale==scale&&expanded.effectiveRoot==root);
        CHECK(expanded.allowedMask==rotateMask(scaleDefinitions()[size_t(scale)].mask,root));
    }
    s.correctionEnabled=false;e.setSettings(s);
    const auto bypass=unpackSnapshot(packSnapshot(e.snapshot()));
    CHECK(!bypass.correctionEnabled&&bypass.allowedMask==0xfff&&bypass.effectiveScale==-1);
}
void reattackLifecycle() {
    Output o;
    // Two different inputs sharing a corrected destination retain BOTH
    // bindings, while each new attack carries its own velocity and timing.
    for(const bool releaseNewestFirst:{false,true}) {
        Engine e;major(e);o.clear();on(e,61,o,4);
        e.beginTimestamp(Source::External,1,1);e.noteOn(Source::External,1,62,73,9,o);
        CHECK(o.size==3&&e.activeVoices()==2);
        CHECK(o.events[0].type==EventType::NoteOn&&o.events[0].note==62&&o.events[0].offset==4);
        CHECK(o.events[1].type==EventType::NoteOff&&o.events[1].note==62&&o.events[1].value==0&&o.events[1].offset==9);
        CHECK(o.events[2].type==EventType::NoteOn&&o.events[2].note==62&&o.events[2].value==73&&o.events[2].offset==9);
        off(e,releaseNewestFirst?62:61,o,12);CHECK(o.size==3&&e.activeVoices()==1);
        off(e,releaseNewestFirst?61:62,o,15);CHECK(o.size==4&&e.activeVoices()==0);
        CHECK(o.events[3].type==EventType::NoteOff&&o.events[3].note==62&&o.events[3].offset==15);
    }
    {
        Engine e;major(e);o.clear();on(e,61,o,1);on(e,61,o,2);
        CHECK(o.size==3&&e.activeVoices()==2);
        off(e,61,o,3);CHECK(o.size==3&&e.activeVoices()==1); // Same-input FIFO.
        off(e,61,o,4);CHECK(o.size==4&&e.activeVoices()==0);
    }
    // Diagnostic collisions remain nonintrusive but own a reference. A real
    // playing note must still reattack a destination started by diagnostics.
    {
        Engine e;major(e);o.clear();on(e,61,o,1);
        on(e,62,o,2,Source::Diagnostic);CHECK(o.size==1&&e.activeVoices()==2);
        e.stopSource(Source::Diagnostic,3,o);CHECK(o.size==1&&e.activeVoices()==1);
        off(e,61,o,4);CHECK(o.size==2&&e.activeVoices()==0);
        o.clear();on(e,61,o,5,Source::Diagnostic);on(e,62,o,6);
        CHECK(o.size==3&&o.events[1].type==EventType::NoteOff&&o.events[2].type==EventType::NoteOn);
        e.stopSource(Source::Diagnostic,7,o);CHECK(o.size==3&&e.activeVoices()==1);
        off(e,62,o,8);CHECK(o.size==4&&e.activeVoices()==0);
    }
    {
        Engine e;major(e);o.clear();on(e,61,o,1);
        on(e,62,o,2,Source::Generated);CHECK(o.size==3);
        e.stopSource(Source::External,3,o);CHECK(o.size==3&&e.activeVoices()==1);
        e.stopSource(Source::Generated,4,o);CHECK(o.size==4&&e.activeVoices()==0);
    }
    {
        Engine e;major(e);o.clear();on(e,61,o,1);
        e.controller(Source::External,1,64,127,2,o);off(e,61,o,3);
        on(e,62,o,4);CHECK(o.size==3&&e.activeVoices()==2);
        e.controller(Source::External,1,64,0,5,o);CHECK(o.size==3&&e.activeVoices()==1);
        off(e,62,o,6);CHECK(o.size==4&&e.activeVoices()==0);
        o.clear();on(e,61,o,7);on(e,62,o,8);o.clear();e.panic(9,o);
        CHECK(o.size==1&&o.events[0].type==EventType::NoteOff&&o.events[0].note==62&&e.activeVoices()==0);
    }
    // Reserving both slots prevents a capacity boundary from emitting only
    // half a retrigger. Exact-fit succeeds; one free slot triggers fail-closed
    // cleanup before creating any new binding, with no partial attack left.
    {
        Engine e;major(e);o.clear();on(e,61,o);o.size=o.events.size()-2;
        on(e,62,o,11);CHECK(!o.overflow&&!e.overloaded()&&o.size==o.events.size()&&e.activeVoices()==2);
        CHECK(o.events[o.size-2].type==EventType::NoteOff&&o.events[o.size-2].value==0);
        CHECK(o.events[o.size-1].type==EventType::NoteOn&&o.events[o.size-1].offset==11);
        o.clear();off(e,61,o,12);CHECK(o.size==0);off(e,62,o,13);CHECK(o.size==1);
        o.clear();on(e,61,o);o.size=o.events.size()-1;on(e,62,o,14);
        CHECK(o.overflow&&e.overloaded()&&e.activeVoices()==0&&o.size==32);
        for(size_t i=0;i<o.size;++i)CHECK(o.events[i].type==EventType::Controller&&o.events[i].offset==14);
        o.clear();off(e,61,o,15);off(e,62,o,16);on(e,60,o,17);CHECK(o.size==0);
        e.panic(18,o);o.clear();on(e,60,o,19);CHECK(o.size==1&&!e.overloaded());
    }
    {
        Engine e;major(e);o.clear();on(e,61,o);o.size=o.events.size();
        on(e,62,o,1,Source::Diagnostic);CHECK(!e.overloaded()&&!o.overflow&&e.activeVoices()==2);
        o.clear();e.stopSource(Source::Diagnostic,2,o);CHECK(o.size==0&&e.activeVoices()==1);
        off(e,61,o,3);CHECK(o.size==1&&e.activeVoices()==0);
    }
}
void noteLifecycle() {
    Engine e;major(e);Output o;
    on(e,61,o,5);CHECK(o.size==1&&o.events[0].note==62&&o.events[0].value==100);
    auto s=e.settings();s.scale=0;e.setSettings(s);
    on(e,61,o,7);CHECK(o.size==2&&o.events[1].note==61);
    off(e,61,o,11);CHECK(o.size==3&&o.events[2].note==62); // FIFO, original mapping
    off(e,61,o,13);CHECK(o.size==4&&o.events[3].note==61);CHECK(e.activeVoices()==0);
    major(e);o.clear();on(e,61,o);on(e,62,o);CHECK(o.size==3);off(e,61,o);CHECK(o.size==3);off(e,62,o);CHECK(o.size==4&&o.events[3].type==EventType::NoteOff);
    o.clear();on(e,61,o);on(e,61,o,0,Source::Generated);CHECK(o.size==3);
    e.stopSource(Source::Generated,9,o);CHECK(o.size==3&&e.activeVoices()==1);
    off(e,61,o,10);CHECK(o.size==4&&o.events[3].offset==10);
    o.clear();on(e,61,o);const int boundBeforeBypass=o.events[0].note;s=e.settings();s.correctionEnabled=false;e.setSettings(s);off(e,61,o);CHECK(o.events[1].note==boundBeforeBypass);
    o.clear();on(e,61,o);CHECK(o.events[0].note==61);e.noteOn(Source::External,1,61,0,12,o);CHECK(o.size==2&&o.events[1].type==EventType::NoteOff);CHECK(e.activeVoices()==0);
    o.clear();on(e,61,o);on(e,61,o,0,Source::External,2);CHECK(o.size==2);off(e,61,o);CHECK(e.activeVoices()==1);off(e,61,o,0,Source::External,2);CHECK(e.activeVoices()==0);
    o.clear();off(e,99,o);CHECK(o.size==0);
    on(e,-1,o);on(e,128,o);on(e,60,o,0,Source::External,0);CHECK(o.size==0);
    chord(e,{60,64,67});on(e,61,o);e.panic(55,o);CHECK(e.activeVoices()==0&&e.snapshot().chordMask==0);CHECK(o.events[o.size-1].type==EventType::NoteOff);
    // Initial no-chord pass-through and later correction retain separate
    // FIFO bindings; the old Off must release its original chromatic pitch.
    Engine fallback;o.clear();on(fallback,61,o);CHECK(o.size==1&&o.events[0].note==61);
    chord(fallback,{60,64,67});on(fallback,61,o);CHECK(o.size==2&&o.events[1].note==60);
    off(fallback,61,o);CHECK(o.size==3&&o.events[2].note==61&&fallback.activeVoices()==1);
    off(fallback,61,o);CHECK(o.size==4&&o.events[3].note==60&&fallback.activeVoices()==0);
    reattackLifecycle();
}
void sustainAndControllers() {
    Engine e;major(e);Output o;
    on(e,61,o);e.controller(Source::External,1,64,127,1,o);off(e,61,o,2);CHECK(o.size==1&&e.activeVoices()==1);
    e.controller(Source::External,1,64,0,3,o);CHECK(o.size==2&&o.events[1].offset==3&&e.activeVoices()==0);
    o.clear();on(e,61,o);e.controller(Source::External,1,64,127,0,o);off(e,61,o);on(e,61,o);off(e,61,o);CHECK(o.size==3&&e.activeVoices()==2);
    e.controller(Source::External,1,64,0,4,o);CHECK(o.size==4&&e.activeVoices()==0);
    o.clear();on(e,61,o);e.controller(Source::External,1,64,127,0,o);off(e,61,o);e.controller(Source::External,1,121,0,6,o);CHECK(o.size==3&&e.activeVoices()==0);CHECK(o.events[2].type==EventType::Controller&&o.events[2].note==121);
    o.clear();on(e,61,o);on(e,61,o,0,Source::Generated);e.controller(Source::External,1,123,0,7,o);CHECK(o.size==3&&e.activeVoices()==1);e.stopSource(Source::Generated,8,o);CHECK(o.size==4);
    o.clear();e.controller(Source::External,2,1,73,19,o);CHECK(o.size==1&&o.events[0].note==1&&o.events[0].value==73&&o.events[0].channel==2&&o.events[0].offset==19);
    // Forwarded hold pedals can sustain receiver notes after core bindings
    // disappear. Global panic must release those pedals with no active voices.
    Engine held;major(held);o.clear();held.panic(0,o);CHECK(o.size==0);
    on(held,60,o);held.controller(Source::External,1,66,127,1,o);
    held.controller(Source::External,2,69,127,2,o);off(held,60,o,3);
    CHECK(held.activeVoices()==0);o.clear();held.panic(21,o);
    CHECK(o.size==2&&o.events[0].channel==1&&o.events[0].note==66&&o.events[0].value==0&&o.events[0].offset==21);
    CHECK(o.events[1].channel==2&&o.events[1].note==69&&o.events[1].value==0);
    o.clear();held.panic(22,o);CHECK(o.size==0);
    held.controller(Source::External,1,66,127,0,o);held.controller(Source::External,1,69,127,0,o);
    held.controller(Source::External,1,121,0,1,o);o.clear();held.panic(2,o);CHECK(o.size==0);
    held.controller(Source::External,3,66,127,0,o);held.controller(Source::External,3,66,0,1,o);
    held.controller(Source::External,3,69,127,0,o);held.controller(Source::External,3,69,0,1,o);
    o.clear();held.panic(2,o);CHECK(o.size==0);
    // Source termination preserves ordinary forwarded expression; only global
    // resets release it. Another source's shared channel must keep its pedal.
    held.controller(Source::External,4,66,127,0,o);o.clear();held.stopSource(Source::Generated,1,o);CHECK(o.size==0);
    held.panic(2,o);CHECK(o.size==1&&o.events[0].channel==4&&o.events[0].note==66);
    // Overflow itself must release remembered pedals, not just All Sound Off.
    o.clear();held.controller(Source::External,5,66,127,0,o);held.controller(Source::External,6,69,127,0,o);
    o.size=o.events.size();on(held,60,o,7);CHECK(o.overflow&&o.size==34);
    CHECK(o.events[32].channel==5&&o.events[32].note==66&&o.events[32].value==0);
    CHECK(o.events[33].channel==6&&o.events[33].note==69&&o.events[33].value==0);
    held.panic(0,o);o.clear();held.controller(Source::External,7,66,127,0,o);
    o.size=o.events.size();held.panic(9,o);CHECK(o.overflow&&o.size==33&&!held.overloaded());
    CHECK(o.events[32].channel==7&&o.events[32].note==66&&o.events[32].value==0);
}
void boundedRealtime() {
    Engine e;major(e);Output o;
    // Saturation is fail-closed, bounded, visible and recoverable. No unbounded
    // tombstone queue: reset is required after overflow before accepting ons.
    for(size_t i=0;i<Engine::voiceCapacity;++i)on(e,60,o);
    CHECK(e.activeVoices()==Engine::voiceCapacity);on(e,60,o,9);CHECK(e.overloaded()&&o.overflow&&e.activeVoices()==0);CHECK(o.size==32);
    for(size_t i=0;i<o.size;++i)CHECK(o.events[i].type==EventType::Controller);
    o.clear();on(e,60,o);CHECK(o.size==0);e.panic(0,o);on(e,60,o);CHECK(o.size==1);off(e,60,o);
    o.clear();o.size=o.events.size();on(e,60,o,11);CHECK(o.overflow&&e.overloaded()&&o.size==32&&e.activeVoices()==0);
    e.panic(0,o);o.clear();
    allocationCount=0;countAllocations=true;
    for(int iteration=0;iteration<10000;++iteration){
        o.clear();e.chordNoteOn(60);e.chordNoteOn(64);e.chordNoteOn(67);e.refreshChord();
        auto s=e.settings();s.mode=Mode(iteration%3);s.root=iteration%12;s.scale=iteration%scaleCount;e.setSettings(s);
        e.beginTimestamp(1);e.noteOn(Source::External,1,iteration%128,100,17,o);
        e.noteOn(Source::Generated,1,(iteration+3)%128,90,18,o);
        e.controller(Source::External,1,64,127,20,o);e.noteOff(Source::External,1,iteration%128,0,21,o);
        e.controller(Source::External,1,64,0,22,o);e.stopSource(Source::Generated,23,o);
        (void)packSnapshot(e.snapshot());e.panic(25,o);
    }
    countAllocations=false;CHECK(allocationCount==0);CHECK(e.activeVoices()==0);
}
}
int main(){
    dataAndRecognition();correction();chordControlAndScales();noteLifecycle();sustainAndControllers();boundedRealtime();
    std::cout<<"Flysnapper core: 6 suites passed, "<<assertions<<" assertions; exhaustive masks/notes and numeric/reference recognition; 10,000 realtime iterations, zero new allocations.\n";
}
