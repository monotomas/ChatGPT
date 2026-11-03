// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Core/MidiEngine.h"
#include "Core/Reference/ChordNames.h"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <set>
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
    e.beginTimestamp(1);e.noteOn(src,channel,n,100,t,out);
}
void off(Engine& e,int n,Output& out,int t=0,Source src=Source::External,int channel=1){e.noteOff(src,channel,n,0,t,out);}
void chord(Engine& e,std::initializer_list<int> notes){e.clearChord();for(auto n:notes)e.chordNoteOn(n);e.refreshChord();}
void dataAndRecognition() {
    CHECK(scaleDefinitions().size()==19);CHECK(chordTypes().size()==50);
    std::set<std::string> ids;
    for(int i=0;i<19;++i){CHECK(ids.insert(scaleDefinitions()[size_t(i)].id).second);CHECK(scaleIndexFromId(scaleDefinitions()[size_t(i)].id)==i);}
    ids.clear();
    for(int i=0;i<50;++i){CHECK(ids.insert(chordTypes()[size_t(i)].id).second);CHECK(chordIndexFromId(chordTypes()[size_t(i)].id)==i);CHECK(factoryDefaults()[size_t(i)]>=0&&factoryDefaults()[size_t(i)]<19);}
    CHECK(scaleIndexFromId("missing")==-1);CHECK(chordIndexFromId("missing")==-1);
    CHECK(scaleDefinitions()[0].mask==0xfff);
    for(const auto& s:scaleDefinitions())for(int root=0;root<12;++root) {
        auto rotated=rotateMask(s.mask,root);
        CHECK(buffkiller::chordBitCount(rotated)==buffkiller::chordBitCount(s.mask));
        for(int pc=0;pc<12;++pc)CHECK(bool(rotated&(1u<<((pc+root)%12)))==bool(s.mask&(1u<<pc)));
    }
    initialiseMusicData();
    // Exhaustive parity with the audited reference, including every supported
    // omission/additional ninth, invalid bass, ambiguity and first candidate.
    for(int mask=0;mask<4096;++mask)for(int bass=0;bass<12;++bass) {
        const auto ref=buffkiller::describeChord(uint16_t(mask),bass);
        if(!(mask&(1u<<bass)))continue; // reference picks a replacement bass
        const auto match=recogniseChord(uint16_t(mask),bass);
        CHECK(match.candidateCount==int(ref.candidates.size()));
        if(ref.candidates.empty()){CHECK(match.typeIndex==-1);CHECK(match.root==-1);}
        else {CHECK(match.typeIndex==ref.candidates.front().patternIndex);CHECK(match.root==ref.candidates.front().root);}
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
    e.beginTimestamp(2);e.noteOn(Source::External,1,68,90,100,o);e.noteOn(Source::Generated,1,70,90,100,o);
    CHECK(o.events[0].note==69);CHECK(o.events[0].offset==100);
}
void chordControlAndScales() {
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
    for(int type=0;type<50;++type)for(int root=0;root<12;++root) {
        const auto mask=rotateMask(buffkiller::correctedChordPatterns[size_t(type)].intervals,root);
        e.clearChord();
        for(int pc=0;pc<12;++pc)if(mask&(1u<<pc))e.chordNoteOn(pc==root?36+pc:60+pc);
        e.refreshChord();const auto match=e.snapshot();CHECK(match.typeIndex>=0);
        for(int scale=0;scale<19;++scale) {
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
    e.clearChord();CHECK(e.snapshot().allowedMask==0);o.clear();on(e,60,o);CHECK(o.size==0);off(e,60,o);CHECK(e.activeVoices()==0);
    chord(e,{60,61});CHECK(e.snapshot().typeIndex==-1);CHECK(e.snapshot().allowedMask==0);
    const auto pack=packSnapshot(e.snapshot());const auto unpack=unpackSnapshot(pack);
    CHECK(unpack.chordMask==e.snapshot().chordMask);CHECK(unpack.allowedMask==e.snapshot().allowedMask);CHECK(unpack.typeIndex==-1);
    CHECK(unpack.effectiveScale==-1&&unpack.effectiveRoot==-1&&unpack.mode==1&&unpack.correctionEnabled);
    s.mode=Mode::FixedScale;s.root=11;s.scale=18;e.setSettings(s);
    const auto fixed=unpackSnapshot(packSnapshot(e.snapshot()));
    CHECK(fixed.effectiveScale==18&&fixed.effectiveRoot==11&&fixed.mode==2&&fixed.correctionEnabled);
    CHECK(fixed.allowedMask==rotateMask(scaleDefinitions()[18].mask,11));
    s.correctionEnabled=false;e.setSettings(s);
    const auto bypass=unpackSnapshot(packSnapshot(e.snapshot()));
    CHECK(!bypass.correctionEnabled&&bypass.allowedMask==0xfff&&bypass.effectiveScale==-1);
}
void noteLifecycle() {
    Engine e;major(e);Output o;
    on(e,61,o,5);CHECK(o.size==1&&o.events[0].note==62&&o.events[0].value==100);
    auto s=e.settings();s.scale=0;e.setSettings(s);
    on(e,61,o,7);CHECK(o.size==2&&o.events[1].note==61);
    off(e,61,o,11);CHECK(o.size==3&&o.events[2].note==62); // FIFO, original mapping
    off(e,61,o,13);CHECK(o.size==4&&o.events[3].note==61);CHECK(e.activeVoices()==0);
    major(e);o.clear();on(e,61,o);on(e,62,o);CHECK(o.size==1);off(e,61,o);CHECK(o.size==1);off(e,62,o);CHECK(o.size==2&&o.events[1].type==EventType::NoteOff);
    o.clear();on(e,61,o);on(e,61,o,0,Source::Generated);CHECK(o.size==1);
    e.stopSource(Source::Generated,9,o);CHECK(o.size==1&&e.activeVoices()==1);
    off(e,61,o,10);CHECK(o.size==2&&o.events[1].offset==10);
    o.clear();on(e,61,o);s=e.settings();s.correctionEnabled=false;e.setSettings(s);off(e,61,o);CHECK(o.events[1].note==62);
    o.clear();on(e,61,o);CHECK(o.events[0].note==61);e.noteOn(Source::External,1,61,0,12,o);CHECK(o.size==2&&o.events[1].type==EventType::NoteOff);CHECK(e.activeVoices()==0);
    o.clear();on(e,61,o);on(e,61,o,0,Source::External,2);CHECK(o.size==2);off(e,61,o);CHECK(e.activeVoices()==1);off(e,61,o,0,Source::External,2);CHECK(e.activeVoices()==0);
    o.clear();off(e,99,o);CHECK(o.size==0);
    on(e,-1,o);on(e,128,o);on(e,60,o,0,Source::External,0);CHECK(o.size==0);
    chord(e,{60,64,67});on(e,61,o);e.panic(55,o);CHECK(e.activeVoices()==0&&e.snapshot().chordMask==0);CHECK(o.events[o.size-1].type==EventType::NoteOff);
    // A suppressed note retains a zero-output binding. Repeating that input
    // after chords arrive must not let the old off terminate the newer voice.
    Engine suppressed;o.clear();on(suppressed,61,o);CHECK(o.size==0);
    chord(suppressed,{60,64,67});on(suppressed,61,o);CHECK(o.size==1);
    off(suppressed,61,o);CHECK(o.size==1&&suppressed.activeVoices()==1);
    off(suppressed,61,o);CHECK(o.size==2&&suppressed.activeVoices()==0);
}
void sustainAndControllers() {
    Engine e;major(e);Output o;
    on(e,61,o);e.controller(Source::External,1,64,127,1,o);off(e,61,o,2);CHECK(o.size==1&&e.activeVoices()==1);
    e.controller(Source::External,1,64,0,3,o);CHECK(o.size==2&&o.events[1].offset==3&&e.activeVoices()==0);
    o.clear();on(e,61,o);e.controller(Source::External,1,64,127,0,o);off(e,61,o);on(e,61,o);off(e,61,o);CHECK(o.size==1&&e.activeVoices()==2);
    e.controller(Source::External,1,64,0,4,o);CHECK(o.size==2&&e.activeVoices()==0);
    o.clear();on(e,61,o);e.controller(Source::External,1,64,127,0,o);off(e,61,o);e.controller(Source::External,1,121,0,6,o);CHECK(o.size==3&&e.activeVoices()==0);CHECK(o.events[2].type==EventType::Controller&&o.events[2].note==121);
    o.clear();on(e,61,o);on(e,61,o,0,Source::Generated);e.controller(Source::External,1,123,0,7,o);CHECK(o.size==1&&e.activeVoices()==1);e.stopSource(Source::Generated,8,o);CHECK(o.size==2);
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
        auto s=e.settings();s.mode=Mode(iteration%3);s.root=iteration%12;s.scale=iteration%19;e.setSettings(s);
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
