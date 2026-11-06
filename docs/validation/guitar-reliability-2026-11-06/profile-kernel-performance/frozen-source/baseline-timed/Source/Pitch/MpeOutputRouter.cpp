// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MpeOutputRouter.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace flysnapper::midi {
namespace {
int sourceIndex(Source source) noexcept { return std::clamp(int(source),0,2); }
int wheelValue(double semitones) noexcept {
    return std::clamp(int(std::lround(8192.+8192.*semitones/MpeOutputRouter::memberRange)),0,16383);
}
}
MpeOutputRouter::MpeOutputRouter() noexcept {
    for(auto& source:input)for(auto& s:source)s.controllers.fill(-1);
    for(size_t i=0;i<slots.size();++i)slots[i].member=int(i)+2;
}
MpeOutputRouter::InputState& MpeOutputRouter::state(Source source,int channel) noexcept {
    return input[size_t(sourceIndex(source))][size_t(std::clamp(channel,1,16)-1)];
}
MpeOutputRouter::Slot* MpeOutputRouter::slotForMember(int member) noexcept {
    return member>=2&&member<=15?&slots[size_t(member-2)]:nullptr;
}
void MpeOutputRouter::beginBlock(Packets& out,int packetBudget) noexcept {
    out.clear();pending={};
    softLimit=std::clamp(packetBudget,64,int(Packets::capacity))-64;
}
void MpeOutputRouter::endBlock(Engine& engine,Output& core,Packets& out,int lastOffset) noexcept {
    checkBudget(engine,core,out,std::max(0,lastOffset));
}
void MpeOutputRouter::append(Packets& out,int status,int a,int b,int length,int offset) noexcept {
    if(out.size>=size_t(softLimit)){out.overflow=true;return;}
    out.events[out.size++]={{{uint8_t(status),uint8_t(std::clamp(a,0,127)),uint8_t(std::clamp(b,0,127))}},length,std::max(0,offset)};
}
void MpeOutputRouter::rpn(Packets& out,int channel,int number,int msb,int lsb,int offset) noexcept {
    for(const auto pair:std::array<std::array<int,2>,6>{{{{101,(number>>7)&127}},{{100,number&127}},{{6,msb}},{{38,lsb}},{{101,127}},{{100,127}}}}) {
        if(number==6&&pair[0]==38)continue; // MCM is a 7-bit RPN.
        append(out,0xb0+channel-1,pair[0],pair[1],3,offset);
    }
}
void MpeOutputRouter::ensureZone(Packets& out,int offset) noexcept {
    if(zoneConfigured&&std::any_of(slots.begin(),slots.end(),[](const Slot& s){return s.sounding;}))return;
    rpn(out,1,6,14,0,offset); // Lower MPE zone: members 2..15.
    rpn(out,1,0,0,0,offset);  // No global bend from the master.
    append(out,0xe0,0,64,3,offset); // Clear a previous legacy master wheel too.
    zoneConfigured=true;
}
void MpeOutputRouter::wheel(Slot& slot,int value,int offset,Packets& out,bool force) noexcept {
    value=std::clamp(value,0,16383);
    if(!force&&slot.lastWheel==value)return;
    append(out,0xe0+slot.member-1,value&127,(value>>7)&127,3,offset);slot.lastWheel=value;
}
void MpeOutputRouter::prepareSlot(Slot& slot,Packets& out,int offset) noexcept {
    ensureZone(out,offset);
    // A new/changed receiver must see the range before every voice.
    rpn(out,slot.member,0,memberRange,0,offset);
    if(slot.source==Source::Generated) {
        append(out,0xd0+slot.member-1,0,0,2,offset);
        append(out,0xb0+slot.member-1,74,64,3,offset);
        auto& pitch=pending[size_t(slot.member-2)];
        const bool initial=pitch.valid&&pitch.offset==offset&&pitch.anchor==slot.anchor;
        wheel(slot,initial?wheelValue(double(pitch.pitch)-slot.anchor):8192,offset,out,true);
        pitch.valid=false;
    } else {
        append(out,0xd0+slot.member-1,inputPressure(slot.source,slot.inputChannel),0,2,offset);
        for(int cc=0;cc<120;++cc)if(cc!=6&&cc!=38&&cc!=64&&cc!=66&&cc!=69&&cc!=98&&cc!=99&&cc!=100&&cc!=101) {
            const int value=inputController(slot.source,slot.inputChannel,cc);
            if(value>=0)append(out,0xb0+slot.member-1,cc,value,3,offset);
        }
        const int program=inputProgram(slot.source,slot.inputChannel);
        if(program>=0)append(out,0xc0+slot.member-1,program,0,2,offset);
        if(inputController(slot.source,slot.inputChannel,74)<0)append(out,0xb0+slot.member-1,74,64,3,offset);
        wheel(slot,wheelValue(inputPitch(slot.source,slot.inputChannel)),offset,out,true);
    }
}
void MpeOutputRouter::releaseSlot(Slot& slot,Packets& out,int offset) noexcept {
    if(slot.sounding) {
        append(out,0x80+slot.member-1,slot.outputNote,0,3,offset);
        wheel(slot,8192,offset,out,true);
        append(out,0xd0+slot.member-1,0,0,2,offset);
        append(out,0xb0+slot.member-1,74,64,3,offset);
    }
    if(slot.binding>=0&&size_t(slot.binding)<bindings.size()) {
        auto& b=bindings[size_t(slot.binding)];
        b.member=0;if(!b.keyDown)b={};
    }
    const int member=slot.member;slot={};slot.member=member;
}
void MpeOutputRouter::consumeCore(Output& core,Packets& out) noexcept {
    for(size_t i=0;i<core.size;++i) {
        const auto& e=core.events[i];auto* slot=slotForMember(e.channel);
        if(e.type==EventType::Controller) {
            append(out,0xb0+e.channel-1,e.note,e.value,3,e.offset);continue;
        }
        if(!slot||!slot->assigned)continue;
        if(e.type==EventType::NoteOn) {
            slot->outputNote=e.note;prepareSlot(*slot,out,e.offset);
            append(out,0x90+slot->member-1,e.note,e.value,3,e.offset);slot->sounding=true;
        } else releaseSlot(*slot,out,e.offset);
    }
    if(core.overflow)out.overflow=true;
    core.size=0;
}
void MpeOutputRouter::clear(Packets& out,int offset) noexcept {
    for(auto& slot:slots)releaseSlot(slot,out,offset);
    bindings={};pending={};zoneConfigured=false;fault=false;
}
void MpeOutputRouter::clearGenerated(Packets& out,int offset) noexcept {
    for(int i=0;i<generatedVoices;++i)releaseSlot(slots[size_t(i)],out,offset);
    pending={};
}
void MpeOutputRouter::fail(Engine& engine,Output& core,Packets& out,int offset) noexcept {
    engine.panic(offset,core);core.clear();
    // Pending messages from this callback may include Note Ons that have not
    // reached the receiver. Use the MPE master's all-notes-off rather than
    // manufacture unmatched individual Offs after discarding those messages.
    out.clear();const int limit=softLimit;softLimit=int(Packets::capacity);
    append(out,0xb0,120,0,3,offset);append(out,0xb0,123,0,3,offset);
    for(auto& slot:slots){wheel(slot,8192,offset,out,true);const int member=slot.member;slot={};slot.member=member;}
    bindings={};pending={};zoneConfigured=false;fault=true;out.overflow=true;softLimit=limit;
}
void MpeOutputRouter::checkBudget(Engine& engine,Output& core,Packets& out,int offset) noexcept {
    if(out.overflow||engine.overloaded())fail(engine,core,out,offset);
}
void MpeOutputRouter::generatedPitch(int id,int anchor,float pitch,int offset,Packets& out) noexcept {
    if(fault||id<0||id>=generatedVoices||anchor<0||anchor>127||!std::isfinite(pitch))return;
    auto& slot=slots[size_t(id)];
    if(slot.assigned&&slot.anchor==anchor) {
        if(slot.sounding)wheel(slot,wheelValue(double(pitch)-anchor),offset,out);
    } else pending[size_t(id)]={true,anchor,offset,pitch};
}
void MpeOutputRouter::generatedOn(Engine& engine,Output& core,int id,int anchor,int velocity,int offset,Packets& out) noexcept {
    if(fault||id<0||id>=generatedVoices||anchor<0||anchor>127||velocity<=0)return;
    auto& slot=slots[size_t(id)];
    if(slot.assigned) {engine.controller(Source::Generated,slot.member,120,0,offset,core);consumeCore(core,out);releaseSlot(slot,out,offset);}
    slot.assigned=true;slot.source=Source::Generated;slot.anchor=anchor;slot.serial=++serial;
    slot.suppressed=engine.snapshot().allowedMask==0;
    engine.noteOn(Source::Generated,slot.member,anchor,velocity,offset,core);consumeCore(core,out);checkBudget(engine,core,out,offset);
}
void MpeOutputRouter::generatedOff(Engine& engine,Output& core,int id,int anchor,int velocity,int offset,Packets& out) noexcept {
    if(fault||id<0||id>=generatedVoices)return;
    auto& slot=slots[size_t(id)];
    if(!slot.assigned||slot.anchor!=anchor)return;
    engine.noteOff(Source::Generated,slot.member,anchor,velocity,offset,core);consumeCore(core,out);
    if(slot.assigned&&!slot.sounding)releaseSlot(slot,out,offset);
    checkBudget(engine,core,out,offset);
}
void MpeOutputRouter::observeExternal(Source source,int channel,const uint8_t* data,int length) noexcept {
    if(source==Source::Generated||channel<1||channel>=16||!data||length<1)return;
    auto& s=state(source,channel);const int status=data[0]&0xf0;
    if(status==0xe0&&length==3)s.wheel=(data[1]&127)|((data[2]&127)<<7);
    else if(status==0xd0&&length==2){s.pressure=data[1]&127;s.pressureOrder=++inputSerial;}
    else if(status==0xc0&&length==2){s.program=data[1]&127;s.programOrder=++inputSerial;}
    else if(status==0xb0&&length==3) {
        const int cc=data[1]&127,value=data[2]&127;s.controllers[size_t(cc)]=value;s.controllerOrder[size_t(cc)]=++inputSerial;
        if(cc==64)s.sustain=value>=64;
        else if(cc==66)s.sostenuto=value>=64;
        else if(cc==69)s.hold2=value>=64;
        else if(cc==101)s.rpnMsb=value;
        else if(cc==100)s.rpnLsb=value;
        else if(cc==6&&s.rpnMsb==0&&s.rpnLsb==0) {
            s.rangeSemitones=std::min(value,96);
            if(inputMember(source,channel))inputZones[size_t(sourceIndex(source))].rangeSemitones=s.rangeSemitones;
        } else if(cc==38&&s.rpnMsb==0&&s.rpnLsb==0) {
            s.rangeCents=std::min(value,99);
            if(inputMember(source,channel))inputZones[size_t(sourceIndex(source))].rangeCents=s.rangeCents;
        } else if(cc==6&&channel==1&&s.rpnMsb==0&&s.rpnLsb==6)
            inputZones[size_t(sourceIndex(source))].members=std::min(value,14);
        else if(cc==98||cc==99)s.rpnMsb=s.rpnLsb=127;
        else if(cc==121)resetInputControllers(source,channel);
    }
}
bool MpeOutputRouter::inputMember(Source source,int channel) const noexcept {
    return channel>=2&&channel<=inputZones[size_t(sourceIndex(source))].members+1;
}
bool MpeOutputRouter::inputTarget(const Slot& slot,Source source,int channel) const noexcept {
    return slot.source==source&&(slot.inputChannel==channel||(channel==1&&inputMember(source,slot.inputChannel)));
}
double MpeOutputRouter::inputPitch(Source source,int channel) const noexcept {
    const auto& s=input[size_t(sourceIndex(source))][size_t(channel-1)];
    if(!inputMember(source,channel))return (s.wheel-8192)*(s.rangeSemitones+s.rangeCents*.01)/8192.;
    const auto& zone=inputZones[size_t(sourceIndex(source))];const auto& master=input[size_t(sourceIndex(source))][0];
    return (s.wheel-8192)*(zone.rangeSemitones+zone.rangeCents*.01)/8192.
        +(master.wheel-8192)*(master.rangeSemitones+master.rangeCents*.01)/8192.;
}
int MpeOutputRouter::inputPressure(Source source,int channel) const noexcept {
    const auto& s=input[size_t(sourceIndex(source))][size_t(channel-1)];const auto& master=input[size_t(sourceIndex(source))][0];
    return inputMember(source,channel)&&master.pressureOrder>s.pressureOrder?master.pressure:s.pressure;
}
int MpeOutputRouter::inputController(Source source,int channel,int cc) const noexcept {
    const auto& s=input[size_t(sourceIndex(source))][size_t(channel-1)];const auto& master=input[size_t(sourceIndex(source))][0];
    return inputMember(source,channel)&&master.controllerOrder[size_t(cc)]>s.controllerOrder[size_t(cc)]?master.controllers[size_t(cc)]:s.controllers[size_t(cc)];
}
int MpeOutputRouter::inputProgram(Source source,int channel) const noexcept {
    const auto& s=input[size_t(sourceIndex(source))][size_t(channel-1)];const auto& master=input[size_t(sourceIndex(source))][0];
    return inputMember(source,channel)&&master.programOrder>s.programOrder?master.program:s.program;
}
void MpeOutputRouter::resetInputControllers(Source source,int channel) noexcept {
    for(int ch=1;ch<=15;++ch)if(ch==channel||(channel==1&&inputMember(source,ch))) {
        auto& s=state(source,ch);const int semitones=s.rangeSemitones,cents=s.rangeCents,program=s.program;
        const int bankMsb=s.controllers[0],bankLsb=s.controllers[32];const auto bankMsbOrder=s.controllerOrder[0],bankLsbOrder=s.controllerOrder[32],programOrder=s.programOrder;
        s={};s.controllers.fill(-1);s.rangeSemitones=semitones;s.rangeCents=cents;s.program=program;
        s.controllers[0]=bankMsb;s.controllers[32]=bankLsb;s.controllerOrder[0]=bankMsbOrder;s.controllerOrder[32]=bankLsbOrder;s.programOrder=programOrder;
        s.pressureOrder=++inputSerial;
    }
}
bool MpeOutputRouter::effectivePedal(const Slot& slot) const noexcept {
    const auto& s=input[size_t(sourceIndex(slot.source))][size_t(slot.inputChannel-1)];
    const auto& master=input[size_t(sourceIndex(slot.source))][0];
    return s.sustain||s.hold2||(s.sostenuto&&slot.latched)
        ||(inputMember(slot.source,slot.inputChannel)&&(master.sustain||master.hold2||(master.sostenuto&&slot.masterLatched)));
}
void MpeOutputRouter::updatePedals(Engine& engine,Output& core,Source source,int channel,int offset,Packets& out) noexcept {
    for(size_t i=generatedVoices;i<slots.size();++i) {
        auto& slot=slots[i];if(!slot.assigned||!inputTarget(slot,source,channel))continue;
        engine.controller(source,slot.member,64,effectivePedal(slot)?127:0,offset,core);consumeCore(core,out);
    }
}
void MpeOutputRouter::externalOff(Engine& engine,Output& core,Source source,int channel,int note,int velocity,int offset,Packets& out) noexcept {
    Binding* chosen=nullptr;
    for(auto& b:bindings)if(b.used&&b.keyDown&&b.source==source&&b.inputChannel==channel&&b.inputNote==note&&(!chosen||b.serial<chosen->serial))chosen=&b;
    if(!chosen)return;
    chosen->keyDown=false;
    auto* slot=slotForMember(chosen->member);
    if(!slot||!slot->assigned){*chosen={};return;}
    engine.noteOff(source,slot->member,note,velocity,offset,core);consumeCore(core,out);
    if(slot->assigned&&slot->suppressed) {
        engine.controller(source,slot->member,120,0,offset,core);consumeCore(core,out);releaseSlot(*slot,out,offset);
    }
}
void MpeOutputRouter::releaseInputChannel(Engine& engine,Output& core,Source source,int channel,int offset,Packets& out) noexcept {
    for(int ch=1;ch<=15;++ch)if(ch==channel||(channel==1&&inputMember(source,ch))) {auto& s=state(source,ch);s.sustain=s.sostenuto=s.hold2=false;}
    for(size_t i=generatedVoices;i<slots.size();++i) {
        auto& slot=slots[i];if(!slot.assigned||!inputTarget(slot,source,channel))continue;
        engine.controller(source,slot.member,120,0,offset,core);consumeCore(core,out);releaseSlot(slot,out,offset);
    }
    for(auto& b:bindings)if(b.used&&b.source==source&&(b.inputChannel==channel||(channel==1&&inputMember(source,b.inputChannel))))b={};
}
void MpeOutputRouter::external(Engine& engine,Output& core,Source source,int channel,const uint8_t* data,int length,int offset,Packets& out) noexcept {
    if(source==Source::Generated||channel<1||channel>=16||!data||length<1)return;
    const int status=data[0]&0xf0,a=length>1?data[1]&127:0,b=length>2?data[2]&127:0;
    if(status<0x80||status>0xe0||length!=((status==0xc0||status==0xd0)?2:3))return;
    if(fault) {
        // A pedal/wheel can physically return while output is latched off.
        // Keep that state current so recovery cannot revive a stale sustain.
        if(status!=0x80&&status!=0x90)observeExternal(source,channel,data,length);
        return;
    }
    if(status==0x80||(status==0x90&&b==0))externalOff(engine,core,source,channel,a,b,offset,out);
    else if(status==0x90&&length==3) {
        auto binding=std::find_if(bindings.begin(),bindings.end(),[](const Binding& value){return !value.used;});
        if(binding==bindings.end()){fail(engine,core,out,offset);return;}
        Slot* selected=nullptr;
        for(size_t i=generatedVoices;i<slots.size();++i)if(!slots[i].assigned){selected=&slots[i];break;}
        if(!selected) {
            for(size_t i=generatedVoices;i<slots.size();++i)if(!selected||slots[i].serial<selected->serial)selected=&slots[i];
            engine.controller(selected->source,selected->member,120,0,offset,core);consumeCore(core,out);releaseSlot(*selected,out,offset);++steals;
        }
        *binding={true,true,source,channel,a,selected->member,++serial};
        selected->assigned=true;selected->source=source;selected->anchor=a;selected->inputChannel=channel;
        selected->binding=int(binding-bindings.begin());selected->serial=serial;selected->suppressed=engine.snapshot().allowedMask==0;
        engine.controller(source,selected->member,64,effectivePedal(*selected)?127:0,offset,core);consumeCore(core,out);
        engine.noteOn(source,selected->member,a,b,offset,core);consumeCore(core,out);
    } else {
        auto& before=state(source,channel);const bool wasSostenuto=before.sostenuto;
        const int oldMembers=inputZones[size_t(sourceIndex(source))].members;
        observeExternal(source,channel,data,length);
        if(inputZones[size_t(sourceIndex(source))].members!=oldMembers) {
            // A host changing its input allocation starts a new input epoch.
            // Release external bindings; generated strings and chord16 survive.
            for(int ch=1;ch<=15;++ch)releaseInputChannel(engine,core,source,ch,offset,out);
        } else if(status==0xb0&&(a==120||a==123))releaseInputChannel(engine,core,source,channel,offset,out);
        else if(status==0xb0&&(a==64||a==66||a==69||a==121)) {
            if(a==66&&b>=64&&!wasSostenuto)for(size_t i=generatedVoices;i<slots.size();++i) {
                auto& slot=slots[i];if(slot.assigned&&inputTarget(slot,source,channel)&&slot.binding>=0) {
                    if(slot.inputChannel==channel)slot.latched=bindings[size_t(slot.binding)].keyDown;
                    else slot.masterLatched=bindings[size_t(slot.binding)].keyDown;
                }
            }
            if((a==66&&b<64)||a==121)for(size_t i=generatedVoices;i<slots.size();++i)if(inputTarget(slots[i],source,channel)) {
                if(slots[i].inputChannel==channel||a==121)slots[i].latched=false;
                if(channel==1)slots[i].masterLatched=false;
            }
            updatePedals(engine,core,source,channel,offset,out);
            if(a==121)for(size_t i=generatedVoices;i<slots.size();++i) {
                auto& slot=slots[i];if(slot.assigned&&inputTarget(slot,source,channel)){
                    append(out,0xb0+slot.member-1,121,0,3,offset);
                    wheel(slot,wheelValue(inputPitch(source,slot.inputChannel)),offset,out,true);
                    append(out,0xd0+slot.member-1,inputPressure(source,slot.inputChannel),0,2,offset);
                    const int timbre=inputController(source,slot.inputChannel,74);
                    append(out,0xb0+slot.member-1,74,timbre<0?64:timbre,3,offset);
                }
            }
        } else if(status==0xe0||(status==0xb0&&(a==6||a==38))) {
            const bool memberRange=status==0xb0&&inputMember(source,channel)&&state(source,channel).rpnMsb==0&&state(source,channel).rpnLsb==0;
            for(size_t i=generatedVoices;i<slots.size();++i)if(slots[i].assigned&&slots[i].source==source
                &&(inputTarget(slots[i],source,channel)||(memberRange&&inputMember(source,slots[i].inputChannel))))
                wheel(slots[i],wheelValue(inputPitch(source,slots[i].inputChannel)),offset,out);
        } else if(status==0xb0&&(a==98||a==99||a==100||a==101)) {
            // Input RPN/NRPN selection must not alter the MPE receiver's zone.
        } else for(size_t i=generatedVoices;i<slots.size();++i) {
            auto& slot=slots[i];if(!slot.assigned||!inputTarget(slot,source,channel))continue;
            const int memberStatus=status+slot.member-1;
            if(status==0xa0) {if(slot.anchor==a&&slot.outputNote>=0)append(out,0xd0+slot.member-1,b,0,2,offset);}
            else append(out,memberStatus,a,b,length,offset);
        }
    }
    checkBudget(engine,core,out,offset);
}
int MpeOutputRouter::activeGenerated() const noexcept {
    return int(std::count_if(slots.begin(),slots.begin()+generatedVoices,[](const Slot& slot){return slot.sounding;}));
}
int MpeOutputRouter::activeExternal() const noexcept {
    return int(std::count_if(slots.begin()+generatedVoices,slots.end(),[](const Slot& slot){return slot.sounding;}));
}
}
