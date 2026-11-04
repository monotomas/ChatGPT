// SPDX-License-Identifier: AGPL-3.0-or-later
// Receiver-level protocol tests: the independent JUCE MPE instrument computes
// actual note pitches, rather than assuming different MIDI channels suffice.
#include "Pitch/MpeOutputRouter.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
using flysnapper::Source;
using flysnapper::midi::MpeOutputRouter;
struct Harness {
    flysnapper::Engine engine;
    flysnapper::Output core;
    MpeOutputRouter router;
    flysnapper::midi::Packets packets;
    juce::MPEInstrument receiver;
    std::vector<juce::MidiMessage> history;
    Harness(){flysnapper::Settings s;s.correctionEnabled=false;engine.setSettings(s);begin();}
    void begin(int budget=2048){router.beginBlock(packets,budget);core.clear();}
    void deliver(){
        router.endBlock(engine,core,packets,127);
        for(size_t i=0;i<packets.size;++i){
            const auto& p=packets.events[i];
            check(p.offset>=0&&p.offset<128,"MPE packet timestamp outside current block");
            juce::MidiMessage m(p.bytes.data(),p.length,double(p.offset));
            check(!m.isNoteOnOrOff()||(m.getChannel()>=2&&m.getChannel()<=15),"MPE emitted note on master/chord channel");
            check(!m.isPitchWheel()||m.getChannel()!=1||m.getPitchWheelValue()==8192,"Non-centred external wheel leaked to MPE master");
            if(m.isNoteOff())check(receiver.getNote(m.getChannel(),m.getNoteNumber()).isValid(),"Router emitted an unmatched Note Off at receiver");
            if(m.isNoteOn())check(!receiver.getNote(m.getChannel(),m.getNoteNumber()).isValid(),"Router reused an occupied MPE member without release");
            receiver.processNextMidiEvent(m);history.push_back(m);
        }
        begin();
    }
    void note(int id,int anchor,float pitch,int offset=0){engine.beginTimestamp(1);router.generatedPitch(id,anchor,pitch,offset,packets);router.generatedOn(engine,core,id,anchor,100,offset,packets);}
    void off(int id,int anchor,int offset=0){router.generatedOff(engine,core,id,anchor,0,offset,packets);}
    void ext(const juce::MidiMessage& m,int offset=0,Source source=Source::External){engine.beginTimestamp(m.isNoteOn()?1:0);router.external(engine,core,source,m.getChannel(),m.getRawData(),m.getRawDataSize(),offset,packets);}
    juce::MPENote get(int member,int anchor){auto note=receiver.getNote(member,anchor);check(note.isValid(),"Expected MPE receiver voice absent");return note;}
    double pitch(int member,int anchor){const auto n=get(member,anchor);return 69+12*std::log2(n.getFrequencyInHertz()/440.);}
    void expect(int member,int anchor,double target){check(std::abs(pitch(member,anchor)-target)<.007,"Receiver pitch differs from independent target");}
};

void independentGeneratedAndKeyboard(){
    auto h=std::make_unique<Harness>();
    h->note(0,57,57.25f);h->note(1,64,64.0f,3);h->deliver();
    auto zone=h->receiver.getZoneLayout().getLowerZone();
    check(zone.numMemberChannels==14&&zone.perNotePitchbendRange==48&&zone.masterPitchbendRange==0,"MPE zone/ranges not established at receiver");
    h->expect(2,57,57.25);h->expect(3,64,64);
    for(const auto pair:std::vector<std::pair<int,int>>{{101,0},{100,0},{6,12},{38,0},{101,127},{100,127}})h->ext(juce::MidiMessage::controllerEvent(1,pair.first,pair.second));
    h->ext(juce::MidiMessage::pitchWheel(1,12288));h->ext(juce::MidiMessage::noteOn(1,72,juce::uint8(91)),7);h->deliver();
    h->expect(8,72,78);h->expect(2,57,57.25);h->expect(3,64,64);
    h->router.generatedPitch(0,57,59.0f,19,h->packets);h->ext(juce::MidiMessage::pitchWheel(1,4096),31);h->deliver();
    h->expect(2,57,59);h->expect(3,64,64);h->expect(8,72,66);
    h->ext(juce::MidiMessage::noteOn(1,77,juce::uint8(82)));h->deliver();h->expect(9,77,71);
    h->ext(juce::MidiMessage::aftertouchChange(1,72,100));h->deliver();
    check(h->get(8,72).pressure.as7BitInt()==100&&h->get(9,77).pressure.as7BitInt()==0&&h->get(2,57).pressure.as7BitInt()==0,"Per-key pressure lost source/note isolation");
    h->ext(juce::MidiMessage::controllerEvent(1,74,90));h->deliver();
    check(h->get(8,72).timbre.as7BitInt()==90&&h->get(9,77).timbre.as7BitInt()==90&&h->get(2,57).timbre.as7BitInt()==64,"Keyboard timbre affected generated string");
    h->ext(juce::MidiMessage::channelPressureChange(1,80));h->deliver();
    check(h->get(8,72).pressure.as7BitInt()==80&&h->get(9,77).pressure.as7BitInt()==80&&h->get(2,57).pressure.as7BitInt()==0,"Keyboard channel pressure affected generated string");
    h->off(0,57);h->off(1,64);h->ext(juce::MidiMessage::noteOff(1,72));h->ext(juce::MidiMessage::noteOff(1,77));h->deliver();
    check(h->receiver.getNumPlayingNotes()==0&&h->engine.activeVoices()==0,"MPE source release left a voice held");
}
void allSixVoicesAndCorrection(){
    auto h=std::make_unique<Harness>();
    const std::array<int,6> notes{{40,45,50,55,59,64}};
    for(int id=0;id<6;++id)h->note(id,notes[size_t(id)],float(notes[size_t(id)]+.2*id),id*2);
    h->deliver();check(h->receiver.getNumPlayingNotes()==6,"Not all six generated voices reached MPE receiver");
    for(int id=0;id<6;++id)h->expect(id+2,notes[size_t(id)],notes[size_t(id)]+.2*id);
    for(int id=0;id<6;++id){h->router.generatedPitch(id,notes[size_t(id)],float(notes[size_t(id)]-1.0-.1*id),id*2,h->packets);}
    h->deliver();for(int id=0;id<6;++id)h->expect(id+2,notes[size_t(id)],notes[size_t(id)]-1.0-.1*id);
    for(int id=0;id<6;++id)h->off(id,notes[size_t(id)]);
    h->deliver();check(h->receiver.getNumPlayingNotes()==0,"Six-voice release left a receiver note");
    flysnapper::Settings s;s.mode=flysnapper::Mode::FixedScale;s.root=0;s.scale=1;h->engine.setSettings(s);
    h->note(0,61,61.25f);h->note(1,62,62);h->deliver();
    h->expect(2,62,62.25);h->expect(3,62,62);
    h->engine.chordNoteOn(48);h->engine.chordNoteOn(52);h->engine.chordNoteOn(55);h->engine.refreshChord();
    s.mode=flysnapper::Mode::ChordNotes;h->engine.setSettings(s);
    h->router.generatedPitch(0,61,62.0f,12,h->packets);h->deliver();h->expect(2,62,63);
    h->off(0,61);h->deliver();check(!h->receiver.getNote(2,62).isValid()&&h->receiver.getNote(3,62).isValid(),"Corrected unison voices collided across members");
    h->off(1,62);h->deliver();check(h->receiver.getNumPlayingNotes()==0,"Corrected anchor Note Off was lost");
}
void sourcePedals(){
    auto h=std::make_unique<Harness>();
    h->ext(juce::MidiMessage::controllerEvent(1,64,127));h->ext(juce::MidiMessage::noteOn(1,60,juce::uint8(90)));h->deliver();
    h->ext(juce::MidiMessage::noteOff(1,60));h->deliver();check(h->receiver.getNumPlayingNotes()==1,"Pedal already held before Note On was ignored");
    h->note(0,57,57.5f);h->ext(juce::MidiMessage::controllerEvent(1,64,0));h->deliver();
    check(!h->receiver.getNote(8,60).isValid()&&h->receiver.getNote(2,57).isValid(),"Keyboard sustain released or retained wrong source");
    h->ext(juce::MidiMessage::noteOn(1,60,juce::uint8(90)));h->ext(juce::MidiMessage::controllerEvent(1,66,127),1);h->ext(juce::MidiMessage::noteOn(1,64,juce::uint8(90)),2);h->deliver();
    h->ext(juce::MidiMessage::noteOff(1,60));h->ext(juce::MidiMessage::noteOff(1,64),1);h->deliver();
    check(h->receiver.getNote(8,60).isValid()&&!h->receiver.getNote(9,64).isValid(),"Sostenuto did not latch only keys already down");
    h->ext(juce::MidiMessage::controllerEvent(1,66,0));h->deliver();check(!h->receiver.getNote(8,60).isValid(),"Sostenuto release left key held");
    h->ext(juce::MidiMessage::controllerEvent(1,69,127));h->ext(juce::MidiMessage::noteOn(1,67,juce::uint8(90)),1);h->ext(juce::MidiMessage::noteOff(1,67),2);h->deliver();check(h->receiver.getNote(8,67).isValid(),"Hold2 lost future keyboard note");
    h->ext(juce::MidiMessage::controllerEvent(1,69,0));h->deliver();check(!h->receiver.getNote(8,67).isValid(),"Hold2 release left key held");
    h->engine.stopSource(Source::Generated,0,h->core);h->router.consumeCore(h->core,h->packets);h->router.clearGenerated(h->packets);h->deliver();
    check(h->receiver.getNumPlayingNotes()==0,"Generated source cleanup left note held");
}
void stealingFifoAndRejectedInput(){
    auto h=std::make_unique<Harness>();
    for(int note=60;note<69;++note){h->ext(juce::MidiMessage::noteOn(1,note,juce::uint8(90)));h->deliver();}
    check(h->receiver.getNumPlayingNotes()==8&&h->router.stolenVoices()==1,"External voice pool did not steal bounded oldest member");
    h->ext(juce::MidiMessage::noteOff(1,60));h->deliver();check(h->receiver.getNote(8,68).isValid(),"Stolen note's later Off cut its replacement");
    for(int note=61;note<69;++note){h->ext(juce::MidiMessage::noteOff(1,note));h->deliver();}
    check(h->receiver.getNumPlayingNotes()==0&&h->engine.activeVoices()==0,"Voice stealing left internal or receiver notes");
    for(int i=0;i<9;++i){h->ext(juce::MidiMessage::noteOn(1,60,juce::uint8(90)));h->deliver();}
    for(int i=0;i<9;++i){h->ext(juce::MidiMessage::noteOff(1,60));h->deliver();check(h->receiver.getNumPlayingNotes()==(i==0?8:8-i),"Stolen same-key FIFO binding lost");}
    h->note(0,57,57);h->deliver();
    const uint8_t malformed[]{0x90},clock[]{0xf8};
    h->router.external(h->engine,h->core,Source::External,1,malformed,1,0,h->packets);
    h->router.external(h->engine,h->core,Source::External,1,clock,1,0,h->packets);
    h->ext(juce::MidiMessage::noteOn(16,60,juce::uint8(90)));h->deliver();check(h->receiver.getNumPlayingNotes()==1,"Malformed/chord-control input disturbed performance voices");
    h->off(0,57);h->deliver();
}
void resetAndOverflow(){
    auto h=std::make_unique<Harness>();
    h->ext(juce::MidiMessage::noteOn(1,60,juce::uint8(90)));h->note(0,57,57.5f);h->deliver();
    h->engine.stopSource(Source::Generated,0,h->core);h->router.consumeCore(h->core,h->packets);h->router.clearGenerated(h->packets);h->deliver();
    check(h->receiver.getNumPlayingNotes()==1&&h->receiver.getNote(8,60).isValid(),"Generated stop cut external member");
    h->note(0,57,57.5f);h->deliver();
    h->engine.panic(0,h->core);h->router.consumeCore(h->core,h->packets);h->router.clear(h->packets);h->deliver();check(h->receiver.getNumPlayingNotes()==0,"Global panic left MPE notes");
    h->note(0,57,57);h->deliver();
    h->ext(juce::MidiMessage::controllerEvent(1,64,127));h->ext(juce::MidiMessage::pitchWheel(1,12288));h->deliver();
    h->begin(64);h->note(1,64,64);h->deliver();
    check(h->router.faulted()&&h->receiver.getNumPlayingNotes()==0&&h->engine.activeVoices()==0,"Packet overflow failed to stop complete MPE zone");
    h->ext(juce::MidiMessage::controllerEvent(1,64,0));h->ext(juce::MidiMessage::pitchWheel(1,8192));h->deliver();
    h->router.clear(h->packets);h->deliver();h->note(0,57,57);h->deliver();check(h->receiver.getNumPlayingNotes()==1,"Panic/reset did not recover output after overflow");
    h->off(0,57);h->deliver();
    h->ext(juce::MidiMessage::noteOn(1,60,juce::uint8(90)));h->deliver();h->expect(8,60,60);
    h->ext(juce::MidiMessage::noteOff(1,60));h->deliver();check(h->receiver.getNumPlayingNotes()==0,"Recovery revived wheel/pedal held before capacity fault");
}
void controllersAndCorrectionSuppression(){
    auto h=std::make_unique<Harness>();
    h->ext(juce::MidiMessage::controllerEvent(1,0,3));h->ext(juce::MidiMessage::controllerEvent(1,32,2));
    h->ext(juce::MidiMessage::programChange(1,7));h->ext(juce::MidiMessage::controllerEvent(1,1,80));
    h->ext(juce::MidiMessage::pitchWheel(1,12288));h->ext(juce::MidiMessage::channelPressureChange(1,70));
    h->ext(juce::MidiMessage::controllerEvent(1,64,127));h->ext(juce::MidiMessage::noteOn(1,60,juce::uint8(90)));h->note(0,57,57.5f);h->deliver();
    h->expect(8,60,61);h->expect(2,57,57.5);
    bool bank=false,program=false,modulation=false;
    for(const auto& m:h->history)if(m.getChannel()==8) {
        if(m.isController()&&m.getControllerNumber()==0&&m.getControllerValue()==3)bank=true;
        if(m.isProgramChange()&&m.getProgramChangeNumber()==7)program=true;
        if(m.isController()&&m.getControllerNumber()==1&&m.getControllerValue()==80)modulation=true;
    }
    check(bank&&program&&modulation,"Future keyboard member lost cached bank/program/controllers");
    h->ext(juce::MidiMessage::controllerEvent(1,121,0));h->deliver();
    h->expect(8,60,60);h->expect(2,57,57.5);
    check(h->get(8,60).pressure.as7BitInt()==0,"Reset controllers did not clear external pressure");
    h->ext(juce::MidiMessage::noteOff(1,60));h->off(0,57);h->deliver();
    check(h->receiver.getNumPlayingNotes()==0,"Reset controllers retained sustain on external member");
    flysnapper::Settings s;s.mode=flysnapper::Mode::ChordScale;h->engine.setSettings(s);
    h->ext(juce::MidiMessage::controllerEvent(1,64,127));h->ext(juce::MidiMessage::noteOn(1,60,juce::uint8(90)));h->note(0,57,57.3f);h->deliver();
    check(h->receiver.getNumPlayingNotes()==0,"Unknown chord correction unexpectedly emitted a note");
    h->ext(juce::MidiMessage::noteOff(1,60));h->off(0,57);h->deliver();
    h->ext(juce::MidiMessage::controllerEvent(1,64,0));h->deliver();
    check(h->engine.activeVoices()==0,"Suppressed notes/pedal left core bindings");
}
void mixedMpeInputAndLateReceiver(){
    auto h=std::make_unique<Harness>();h->note(0,57,57.25f);
    // The selected MPE contract is robust when a host redistributes playing
    // notes to members without emitting a fresh zone announcement each time.
    h->ext(juce::MidiMessage::noteOn(3,60,juce::uint8(90)));h->ext(juce::MidiMessage::pitchWheel(3,9216));h->deliver();
    h->expect(8,60,66);h->expect(2,57,57.25);
    h->ext(juce::MidiMessage::pitchWheel(1,12288));h->ext(juce::MidiMessage::controllerEvent(1,64,127));h->deliver();
    h->expect(8,60,67);h->expect(2,57,57.25);
    h->ext(juce::MidiMessage::noteOff(3,60));h->ext(juce::MidiMessage::noteOn(4,65,juce::uint8(90)));h->deliver();
    check(h->receiver.getNote(8,60).isValid(),"MPE master sustain failed to hold member input");h->expect(9,65,66);
    h->ext(juce::MidiMessage::channelPressureChange(1,70));h->ext(juce::MidiMessage::controllerEvent(1,74,90));h->deliver();
    check(h->get(8,60).pressure.as7BitInt()==70&&h->get(9,65).pressure.as7BitInt()==70&&h->get(2,57).pressure.as7BitInt()==0,"MPE input master pressure source isolation failed");
    h->ext(juce::MidiMessage::channelPressureChange(4,40));h->ext(juce::MidiMessage::controllerEvent(4,74,80));h->ext(juce::MidiMessage::noteOn(4,72,juce::uint8(90)));h->deliver();
    check(h->get(10,72).pressure.as7BitInt()==40&&h->get(10,72).timbre.as7BitInt()==80,"Future MPE member lost newer per-member expression");
    h->ext(juce::MidiMessage::controllerEvent(4,121,0));h->deliver();
    check(h->get(9,65).timbre.as7BitInt()==90&&h->get(10,72).timbre.as7BitInt()==90,"Member controller reset failed to restore inherited master timbre");
    h->ext(juce::MidiMessage::controllerEvent(1,64,0));h->deliver();check(!h->receiver.getNote(8,60).isValid(),"MPE master sustain release left member note held");
    // Per-note RPN on first member configures sensitivity across the zone.
    for(const auto pair:std::vector<std::pair<int,int>>{{101,0},{100,0},{6,12},{38,0},{101,127},{100,127}})h->ext(juce::MidiMessage::controllerEvent(2,pair.first,pair.second));
    h->ext(juce::MidiMessage::pitchWheel(4,9216));h->deliver();h->expect(9,65,67.5);h->expect(10,72,74.5);h->expect(2,57,57.25);
    h->ext(juce::MidiMessage::controllerEvent(1,121,0));h->deliver();h->expect(9,65,65);h->expect(10,72,72);h->expect(2,57,57.25);
    // Explicit zone disable starts a new external epoch without cutting audio.
    for(const auto pair:std::vector<std::pair<int,int>>{{101,0},{100,6},{6,0},{101,127},{100,127}})h->ext(juce::MidiMessage::controllerEvent(1,pair.first,pair.second));
    h->deliver();check(h->receiver.getNumPlayingNotes()==1&&h->receiver.getNote(2,57).isValid(),"Input zone change cut generated string or left external bindings");
    h->ext(juce::MidiMessage::noteOn(3,60,juce::uint8(90)));h->ext(juce::MidiMessage::pitchWheel(3,9216));h->deliver();h->expect(8,60,60.25);
    h->ext(juce::MidiMessage::noteOff(3,60));h->off(0,57);h->deliver();
    // A receiver inserted between phrases needs MCM as well as member range.
    juce::MPEInstrument replacement;h->note(0,57,58.f);h->router.endBlock(h->engine,h->core,h->packets,127);
    for(size_t i=0;i<h->packets.size;++i){const auto& p=h->packets.events[i];replacement.processNextMidiEvent(juce::MidiMessage(p.bytes.data(),p.length,double(p.offset)));}
    auto heard=replacement.getNote(2,57);check(heard.isValid()&&std::abs(69+12*std::log2(heard.getFrequencyInHertz()/440.)-58)<.007,"Newly inserted MPE receiver missed zone announcement between phrases");
    h->deliver();h->off(0,57);h->deliver();
}
void randomizedProtocolLifecycle(){
    auto h=std::make_unique<Harness>();std::mt19937 random(0x71a5b00u);
    std::array<int,6> anchors{{40,45,50,55,59,64}};
    for(int step=0;step<6000;++step){
        const int action=int(random()%12),channel=1+int(random()%15),note=36+int(random()%49),id=int(random()%6);
        if(action<3)h->ext(juce::MidiMessage::noteOn(channel,note,juce::uint8(40+random()%88)));
        else if(action<5)h->ext(juce::MidiMessage::noteOff(channel,note));
        else if(action==5)h->ext(juce::MidiMessage::pitchWheel(channel,int(random()%16384)));
        else if(action==6){const std::array<int,6> controls{{64,66,69,120,121,123}};h->ext(juce::MidiMessage::controllerEvent(channel,controls[random()%controls.size()],random()%2?127:0));}
        else if(action==7){anchors[size_t(id)]=note;h->note(id,note,float(note)+(int(random()%201)-100)*.01f);}
        else if(action==8)h->router.generatedPitch(id,anchors[size_t(id)],float(anchors[size_t(id)])+(int(random()%801)-400)*.01f,0,h->packets);
        else if(action==9)h->off(id,anchors[size_t(id)]);
        else if(action==10)h->ext(juce::MidiMessage::channelPressureChange(channel,int(random()%128)));
        else {
            const bool zone=(random()%3)==0;const int target=zone?1:channel;
            const int value=zone?std::array<int,3>{{0,6,14}}[random()%3]:std::array<int,3>{{2,12,48}}[random()%3];
            for(const auto pair:std::vector<std::pair<int,int>>{{101,0},{100,zone?6:0},{6,value},{101,127},{100,127}})h->ext(juce::MidiMessage::controllerEvent(target,pair.first,pair.second));
        }
        h->deliver();check(h->receiver.getNumPlayingNotes()<=14,"Randomized input exceeded fourteen physical MPE members");
        if(h->router.faulted()){h->engine.panic(0,h->core);h->router.consumeCore(h->core,h->packets);h->router.clear(h->packets);h->deliver();}
    }
    h->engine.panic(0,h->core);h->router.consumeCore(h->core,h->packets);h->router.clear(h->packets);h->deliver();
    check(h->receiver.getNumPlayingNotes()==0&&h->engine.activeVoices()==0,"Randomized sustain/steal/reconfigure sequence survived panic");
}
}
int main(){try{
    independentGeneratedAndKeyboard();allSixVoicesAndCorrection();sourcePedals();stealingFifoAndRejectedInput();resetAndOverflow();controllersAndCorrectionSuppression();mixedMpeInputAndLateReceiver();randomizedProtocolLifecycle();
    std::cout<<"FlysnapperMpeOutput: PASS ("<<checks<<" checks; actual JUCE MPE receiver; not Live/physical-latency proof).\n";return 0;
}catch(const std::exception& e){std::cerr<<"FlysnapperMpeOutput: FAIL: "<<e.what()<<'\n';return 1;}}
