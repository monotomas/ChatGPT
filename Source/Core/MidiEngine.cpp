// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MidiEngine.h"
#include <algorithm>
#include <cstdlib>
#include <limits>

namespace flysnapper {
static_assert(scaleCount<=63,"Packed GUI status supports at most63 scales plus its empty sentinel");
uint64_t packSnapshot(const Snapshot& s) noexcept {
    // The sixth scale bit uses previously unused bit58. Existing fields and
    // all status words for the original31 scales retain their bit positions.
    // This is transient GUI status, not the XML/preset serialization format.
    return uint64_t(s.chordMask&0xfff)|(uint64_t(s.allowedMask&0xfff)<<12)
        |(uint64_t((s.root+1)&15)<<24)|(uint64_t((s.typeIndex+1)&63)<<28)
        |(uint64_t(std::clamp(s.candidateCount,0,255))<<34)|(uint64_t((s.bass+1)&15)<<42)
        |(uint64_t((s.effectiveScale+1)&31)<<46)|(uint64_t((s.effectiveRoot+1)&15)<<51)
        |(uint64_t(s.mode&3)<<55)|(uint64_t(s.correctionEnabled)<<57)
        |(uint64_t((s.effectiveScale+1)&32)<<53);
}
Snapshot unpackSnapshot(uint64_t s) noexcept {
    return {uint16_t(s&0xfff),uint16_t((s>>12)&0xfff),int((s>>24)&15)-1,
        int((s>>28)&63)-1,int((s>>34)&255),int((s>>42)&15)-1,
        int(((s>>46)&31)|((s>>53)&32))-1,int((s>>51)&15)-1,int((s>>55)&3),bool((s>>57)&1)};
}
std::string formatChord(const Snapshot& s){return chordName(s.chordMask,s.bass);}
Engine::Engine(){initialiseMusicData();refreshChord();}
void Engine::setSettings(const Settings& s) noexcept {
    bool changed=config.correctionEnabled!=s.correctionEnabled||config.mode!=s.mode||config.root!=s.root||config.scale!=s.scale||config.chordScales!=s.chordScales;
    config=s;config.root=(config.root%12+12)%12;config.scale=std::clamp(config.scale,0,scaleCount-1);
    if(int(config.mode)<0||int(config.mode)>2)config.mode=Mode::ChordNotes;
    for(auto& i:config.chordScales)i=std::clamp(i,0,scaleCount-1);
    if(changed)resetHistory();
    updateAllowed();
}
void Engine::chordNoteOn(int note,int velocity) noexcept {
    if(note<0||note>127)return;
    if(velocity<=0){chordNoteOff(note);return;}
    auto& count=chordHeld[size_t(note)];
    if(count<std::numeric_limits<uint16_t>::max())++count;
}
void Engine::chordNoteOff(int note) noexcept {
    if(note<0||note>127)return;
    auto& count=chordHeld[size_t(note)];if(count)--count;
}
void Engine::clearChord() noexcept {chordHeld.fill(0);refreshChord();}
void Engine::refreshChord() noexcept {
    view.chordMask=0;view.bass=-1;
    for(int note=0;note<128;++note)if(chordHeld[size_t(note)]) {
        if(view.bass<0)view.bass=note%12;
        view.chordMask=uint16_t(view.chordMask|(1u<<(note%12)));
    }
    const auto match=recogniseChord(view.chordMask,view.bass);
    view.root=match.root;view.typeIndex=match.typeIndex;view.candidateCount=match.candidateCount;
    updateAllowed();
}
void Engine::updateAllowed() noexcept {
    view.mode=int(config.mode);view.correctionEnabled=config.correctionEnabled;
    view.effectiveScale=-1;view.effectiveRoot=-1;
    if(!config.correctionEnabled){view.allowedMask=0xfff;return;}
    switch(config.mode) {
    case Mode::ChordNotes:view.allowedMask=view.chordMask;break;
    case Mode::ChordScale:
        if(view.typeIndex>=0){view.effectiveScale=config.chordScales[size_t(view.typeIndex)];view.effectiveRoot=view.root;}
        view.allowedMask=view.typeIndex>=0?rotateMask(scaleDefinitions()[size_t(config.chordScales[size_t(view.typeIndex)])].mask,view.root):0;break;
    case Mode::FixedScale:
        view.effectiveScale=config.scale;view.effectiveRoot=config.root;
        view.allowedMask=rotateMask(scaleDefinitions()[size_t(config.scale)].mask,config.root);break;
    }
}
void Engine::beginTimestamp(int noteOnCount) noexcept {
    ambiguous=noteOnCount>1;
    if(ambiguous)resetHistory();
}
int Engine::nearest(int note,uint16_t allowed,int prev,int before) noexcept {
    if(note<0||note>127||!(allowed&0xfff))return -1;
    if(allowed&(1u<<(note%12)))return note;
    int lo=-1,hi=-1;
    for(int d=1;d<128;++d){
        if(note-d>=0&&(allowed&(1u<<((note-d)%12))))lo=note-d;
        if(note+d<=127&&(allowed&(1u<<((note+d)%12))))hi=note+d;
        if(lo>=0||hi>=0)break;
    }
    if(lo<0)return hi;
    if(hi<0)return lo;
    if(prev>=0&&before>=0){
        if(prev<before)return lo;
        if(prev>before)return hi;
        if(note==prev-1)return lo;
    }
    return hi;
}
bool Engine::emit(const Event& e,Output& out) noexcept {
    if(out.size>=out.events.size()){failSafe(e.offset,out);return false;}
    out.events[out.size++]=e;return true;
}
void Engine::failSafe(int offset,Output& out) noexcept {
    // Discard pending output (including potentially unmatched ons), terminate
    // receiver voices, and require explicit panic/transport reset to recover.
    out.size=0;out.overflow=true;fault=true;
    for(int ch=1;ch<=16;++ch){
        out.events[out.size++]={EventType::Controller,ch,120,0,offset};
        out.events[out.size++]={EventType::Controller,ch,123,0,offset};
    }
    // Fixed upper bound: at most 32 extra hold releases, without recursion
    // through emit() while already recovering from output saturation.
    for(int ch=1;ch<=16;++ch) {
        const auto holds=forwardedHolds[size_t(ch-1)];
        if(holds&1)out.events[out.size++]={EventType::Controller,ch,66,0,offset};
        if(holds&2)out.events[out.size++]={EventType::Controller,ch,69,0,offset};
    }
    forwardedHolds={};
    voices={};outputRefs={};pedal={};resetHistory();
}
void Engine::noteOn(Source source,int channel,int note,int velocity,int offset,Output& out) noexcept {
    if(velocity<=0){noteOff(source,channel,note,0,offset,out);return;}
    if(fault||channel<1||channel>16||note<0||note>127)return;
    auto found=std::find_if(voices.begin(),voices.end(),[](const Voice& v){return !v.active;});
    if(found==voices.end()){failSafe(offset,out);return;}
    // A second overlapping input is polyphonic unless the same-note FIFO case
    // can retain direction. Ambiguity never adds lookahead or buffering.
    const bool overlap=std::any_of(voices.begin(),voices.end(),[&](const Voice& v){return v.active&&!v.released&&(v.source!=source||v.input!=note);});
    if(overlap)resetHistory();
    const int output=nearest(note,view.allowedMask,ambiguous?-1:previous,ambiguous?-1:beforePrevious);
    *found={++serial,note,output,channel,source,true,false};
    if(output>=0){
        auto& refs=outputRefs[size_t(channel-1)][size_t(output)];
        if(refs++==0&&!emit({EventType::NoteOn,channel,output,std::clamp(velocity,1,127),offset},out))return;
    }
    if(!ambiguous){beforePrevious=previous;previous=note;}
}
void Engine::release(Voice& v,int velocity,int offset,Output& out) noexcept {
    if(!v.active)return;
    const int output=v.output,channel=v.channel;
    v.active=false;
    if(output>=0){
        auto& refs=outputRefs[size_t(channel-1)][size_t(output)];
        if(refs&&--refs==0)emit({EventType::NoteOff,channel,output,std::clamp(velocity,0,127),offset},out);
    }
}
void Engine::noteOff(Source source,int channel,int note,int velocity,int offset,Output& out) noexcept {
    if(channel<1||channel>16||note<0||note>127)return;
    Voice* found=nullptr;
    for(auto& v:voices)if(v.active&&!v.released&&v.source==source&&v.channel==channel&&v.input==note)
        if(!found||v.serial<found->serial)found=&v;
    if(!found)return;
    if(pedal[size_t(source)][size_t(channel-1)])found->released=true;
    else release(*found,velocity,offset,out);
}
void Engine::controller(Source source,int channel,int cc,int value,int offset,Output& out) noexcept {
    if(fault||channel<1||channel>16||cc<0||cc>127||value<0||value>127)return;
    if(cc==64){
        pedal[size_t(source)][size_t(channel-1)]=value>=64;
        if(value<64)for(auto& v:voices)if(v.active&&v.released&&v.source==source&&v.channel==channel)release(v,0,offset,out);
        return;
    }
    if(cc==120||cc==123){
        for(auto& v:voices)if(v.active&&v.source==source&&v.channel==channel)release(v,0,offset,out);
        pedal[size_t(source)][size_t(channel-1)]=false;resetHistory();return;
    }
    if(cc==121){
        pedal[size_t(source)][size_t(channel-1)]=false;
        for(auto& v:voices)if(v.active&&v.released&&v.source==source&&v.channel==channel)release(v,0,offset,out);
    }
    if(emit({EventType::Controller,channel,cc,value,offset},out)) {
        auto& holds=forwardedHolds[size_t(channel-1)];
        if(cc==121)holds=0;
        else if(cc==66||cc==69) {
            const auto bit=uint8_t(cc==66?1:2);
            holds=value>=64?uint8_t(holds|bit):uint8_t(holds&~bit);
        }
    }
}
void Engine::stopSource(Source source,int offset,Output& out) noexcept {
    for(auto& v:voices)if(v.active&&v.source==source)release(v,0,offset,out);
    pedal[size_t(source)]={};resetHistory();
}
void Engine::panic(int offset,Output& out) noexcept {
    for(auto& v:voices)if(v.active)release(v,0,offset,out);
    releaseForwardedHolds(offset,out);
    voices={};outputRefs={};pedal={};fault=false;serial=0;resetHistory();
    clearChord();
}
void Engine::releaseForwardedHolds(int offset,Output& out) noexcept {
    for(int ch=1;ch<=16;++ch) {
        auto& holds=forwardedHolds[size_t(ch-1)];
        if(holds&1) {
            if(!emit({EventType::Controller,ch,66,0,offset},out))return;
            holds=uint8_t(holds&~1u);
        }
        if(holds&2) {
            if(!emit({EventType::Controller,ch,69,0,offset},out))return;
            holds=uint8_t(holds&~2u);
        }
    }
}
size_t Engine::activeVoices() const noexcept {
    size_t n=0;for(const auto& v:voices)if(v.active)++n;return n;
}
}
