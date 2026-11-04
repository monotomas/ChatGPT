// SPDX-License-Identifier: AGPL-3.0-or-later
// Actual sidechain samples through the processor and an independent JUCE MPE
// receiver. These checks establish protocol and signal decisions, not Live's
// channel preservation or an audio interface's end-to-end latency.
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks=0;
void check(bool condition,const std::string& message){++checks;if(!condition)throw std::runtime_error(message);}
void set(FlysnapperProcessor& p,const char* id,float value){auto* v=p.parameters.getParameter(id);check(v!=nullptr,std::string("Missing MPE parameter ")+id);v->setValueNotifyingHost(v->convertTo0to1(value));}
float get(FlysnapperProcessor& p,const char* id){auto* v=p.parameters.getRawParameterValue(id);check(v!=nullptr,std::string("Missing MPE value ")+id);return v->load();}
constexpr double pi=3.14159265358979323846;
using Signal=std::function<float(int64_t)>;
const Signal silence=[](int64_t){return 0.f;};
struct Raw {int64_t sample;juce::MidiMessage midi;};
juce::MidiMessage on(int note,int channel=1){return juce::MidiMessage::noteOn(channel,note,juce::uint8(96));}
juce::MidiMessage off(int note,int channel=1){return juce::MidiMessage::noteOff(channel,note);}
struct Harness {
    std::unique_ptr<FlysnapperProcessor> processor=std::make_unique<FlysnapperProcessor>();
    juce::MPEInstrument receiver;
    double rate=48000;int64_t position=0;
    std::vector<Raw> events;
    std::array<std::array<int,128>,16> active{};
    Harness(bool audio=false){
        auto layout=processor->getBusesLayout();layout.inputBuses.set(0,audio?juce::AudioChannelSet::stereo():juce::AudioChannelSet::disabled());
        check(processor->setBusesLayout(layout),"Cannot activate processor sidechain");
        set(*processor,"pitchEnabled",audio?1.f:0.f);set(*processor,"mode",2);set(*processor,"scale",0);
        set(*processor,"mpeOutput",1);processor->prepareToPlay(rate,257);
    }
    void block(int count,const Signal& signal=silence,const std::vector<Raw>& input={},bool bypass=false){
        juce::AudioBuffer<float> audio(2,count);juce::MidiBuffer midi;midi.ensureSize(131072);
        for(int n=0;n<count;++n)for(int ch=0;ch<2;++ch)audio.setSample(ch,n,signal(position+n));
        for(const auto& e:input)if(e.sample>=position&&e.sample<position+count)midi.addEvent(e.midi,int(e.sample-position));
        if(bypass)processor->processBlockBypassed(audio,midi);else processor->processBlock(audio,midi);
        check(midi.getNumEvents()<=2048,"Processor exceeded actual VST3 event adapter capacity");
        for(int ch=0;ch<2;++ch)if(count>0)check(audio.getMagnitude(ch,0,count)==0,"MPE path leaked sidechain audio");
        for(const auto e:midi){
            check(e.samplePosition>=0&&(count==0?e.samplePosition==0:e.samplePosition<count),"MPE event outside delivered block");
            const auto m=e.getMessage();
            if(m.isNoteOnOrOff()){
                check(m.getChannel()!=16,"Chord control note escaped processor");
                auto& held=active[size_t(m.getChannel()-1)][size_t(m.getNoteNumber())];
                if(m.isNoteOn())++held;else {check(held>0,"Processor emitted unmatched MPE/legacy Note Off");--held;}
            }
            if((m.isAllNotesOff()||m.isAllSoundOff())&&m.getChannel()==1&&get(*processor,"mpeOutput")>=.5f)active={};
            receiver.processNextMidiEvent(m);events.push_back({position+e.samplePosition,m});
        }
        position+=count;
    }
    void run(int64_t frames,const Signal& signal=silence,const std::vector<Raw>& input={}){
        const std::array<int,4> pattern{{17,64,257,128}};size_t i=0;
        while(frames>0){const int n=int(std::min<int64_t>(frames,pattern[i++%pattern.size()]));block(n,signal,input);frames-=n;}
    }
    juce::MPENote note(int member,int anchor){auto n=receiver.getNote(member,anchor);check(n.isValid(),"Expected processor-to-MPE receiver note missing");return n;}
    double pitch(int member,int anchor){return 69+12*std::log2(note(member,anchor).getFrequencyInHertz()/440.);}
    void expect(int member,int anchor,double target,double tolerance=.007){check(std::abs(pitch(member,anchor)-target)<tolerance,"Processor-to-receiver expressive pitch mismatch");}
    std::vector<Raw> notes(bool isOn,int channel=0)const{std::vector<Raw> out;for(const auto& e:events)if((channel==0||e.midi.getChannel()==channel)&&(isOn?e.midi.isNoteOn():e.midi.isNoteOff()))out.push_back(e);return out;}
    void released()const{for(const auto& channel:active)for(int held:channel)check(held==0,"Processor left raw output note held");check(receiver.getNumPlayingNotes()==0,"Independent MPE receiver left note held");}
};

void chordIsolationAndModeChanges(){
    Harness h;set(*h.processor,"mode",0);
    h.block(128,silence,{{0,on(48,16)},{0,on(52,16)},{0,on(55,16)},{7,on(61)}});
    h.expect(8,60,60);check(h.processor->getSnapshot().chordMask==145,"Channel16 C chord was not analysed");
    h.block(128,silence,{{135,juce::MidiMessage::pitchWheel(1,12288)}});h.expect(8,60,61);
    set(*h.processor,"mpeOutput",0);h.block(128);
    check(h.receiver.getNumPlayingNotes()==0,"MPE mode exit left member notes held");
    check(h.processor->getSnapshot().chordMask==145,"Changing output format discarded held chord input");
    const auto at=h.position;h.block(128,silence,{{at+11,on(61)}});
    auto legacy=h.notes(true,1);check(legacy.size()==1&&legacy.back().midi.getNoteNumber()==60&&legacy.back().sample==at+11,"Legacy format after MPE mode did not restore sourcechannel1 correction");
    set(*h.processor,"mpeOutput",1);h.block(128);
    check(h.active[0][60]==0,"Re-entering MPE omitted legacy Note Off");
    const auto later=h.position;h.block(128,silence,{{later,on(61)},{later+64,off(61)}});h.released();
}
void heldExternalStateAndBypass(){
    Harness h;set(*h.processor,"mpeOutput",0);
    std::vector<Raw> setup;for(const auto pair:std::array<std::array<int,2>,6>{{{{101,0}},{{100,0}},{{6,12}},{{38,0}},{{101,127}},{{100,127}}}})setup.push_back({0,juce::MidiMessage::controllerEvent(1,pair[0],pair[1])});
    setup.push_back({1,juce::MidiMessage::pitchWheel(1,12288)});setup.push_back({2,juce::MidiMessage::controllerEvent(1,64,127)});h.block(128,silence,setup);
    set(*h.processor,"mpeOutput",1);h.block(128,silence,{{128,on(72)},{190,off(72)}});h.expect(8,72,78);
    h.block(128,silence,{{256,juce::MidiMessage::controllerEvent(1,64,0)}});h.released();
    // Bypass still uses the selected wire format, while disabling correction
    // and audio acquisition; switching receiver protocol on bypass is unsafe.
    set(*h.processor,"scale",1);h.block(128,silence,{{384,on(61)},{420,off(61)}},true);
    const auto all=h.notes(true);check(all.back().midi.getChannel()==8&&all.back().midi.getNoteNumber()==61,"Bypass changed selected MPE format or retained pitch correction");h.released();
}
Signal bentTone(double rate,int anchor,double depth,double seconds=.9){
    auto data=std::make_shared<std::vector<float>>(size_t(rate*(seconds+.22)),0.f);double phase=0;
    for(size_t n=0;n<data->size();++n){const double t=n/rate;if(t>=seconds)break;
        const double pitch=anchor+depth*std::clamp((t-.18)/.35,0.,1.);
        phase+=2*pi*440*std::pow(2.,(pitch-69)/12.)/rate;
        const double envelope=.23*std::min(1.,t/.002)*std::exp(-.4*t);
        (*data)[n]=float(envelope*(std::sin(phase)+.23*std::sin(2*phase+.2)+.09*std::sin(3*phase+.6)));
    }
    return [data](int64_t n){return n>=0&&size_t(n)<data->size()?(*data)[size_t(n)]:0.f;};
}
void actualMonoAudioAndSourceShutdown(){
    Harness h(true);set(*h.processor,"monoBend",0);const auto wave=bentTone(h.rate,57,2);
    const std::vector<Raw> input{{1000,on(84)},{2000,juce::MidiMessage::pitchWheel(1,12288)}};
    int samples=0;double maxError=0;
    while(h.position<36000){h.block(128,wave,input);if(h.position>28500&&h.position<35500){maxError=std::max(maxError,std::abs(h.pitch(2,57)-59));h.expect(8,84,85);++samples;}}
    const auto generated=h.notes(true,2);check(generated.size()==1&&generated.front().midi.getNoteNumber()==57,"MPE did not enable mono continuous expression independently of legacy Mono Bend toggle");
    check(generated.front().sample/h.rate<.040,"Mono MPE acquisition exceeded 40ms signal decision budget");
    check(samples>=8&&maxError<.18,"Actual mono audio bend did not reach accurate independent receiver pitch");
    set(*h.processor,"pitchEnabled",0);h.block(128,wave);
    check(!h.receiver.getNote(2,57).isValid()&&h.receiver.getNote(8,84).isValid(),"Stopping generated audio cut external MPE member");
    h.processor->requestPanic();h.block(128);h.released();
    std::cout<<"MEASURE processor MPE mono first decision "<<1000*generated.front().sample/h.rate<<"ms; settled max error "<<100*maxError<<" cents; not end-to-end latency\n";
}
Signal twoStringHammer(double rate){
    auto data=std::make_shared<std::vector<float>>(size_t(rate*1.15),0.f);double movingPhase=0,fixedPhase=0;
    for(size_t n=0;n<data->size();++n){const double t=n/rate;if(t>=.9)break;
        const double changing=t<.4?45.:57.;
        movingPhase+=2*pi*440*std::pow(2.,(changing-69)/12.)/rate;
        fixedPhase+=2*pi*440*std::pow(2.,(64.-69)/12.)/rate;
        const auto string=[](double p){return std::sin(p)+.20*std::sin(2*p+.2)+.08*std::sin(3*p+.4);};
        const double envelope=std::min(1.,t/.002)*std::exp(-.18*t);
        (*data)[n]=float(envelope*(.15*string(movingPhase)+.11*string(fixedPhase)));
    }
    return [data](int64_t n){return n>=0&&size_t(n)<data->size()?(*data)[size_t(n)]:0.f;};
}
void actualPolyHammerKeepsOtherString(){
    Harness h(true);set(*h.processor,"pitchMode",1);const auto wave=twoStringHammer(h.rate);
    h.run(55000,wave);
    const auto ons=h.notes(true);int original=-1,held=-1,replacement=-1;int64_t changedAt=-1;
    for(const auto& e:ons){const int note=e.midi.getNoteNumber();if(note==45&&e.sample<19200)original=e.midi.getChannel();if(note==64&&e.sample<19200)held=e.midi.getChannel();if(note==57&&e.sample>=19200&&changedAt<0){replacement=e.midi.getChannel();changedAt=e.sample;}}
    check(original>=2&&original<=7&&held>=2&&held<=7&&original!=held,"Poly source did not acquire both independent strings");
    check(replacement==original&&changedAt-19200<3840,"Octave hammer was treated as continuous bend or lost stable voice identity");
    bool releasedOriginal=false;
    for(const auto& e:h.notes(false)){
        if(e.midi.getChannel()==original&&e.midi.getNoteNumber()==45&&e.sample>=19200&&e.sample<=changedAt)releasedOriginal=true;
        if(e.midi.getChannel()==held&&e.midi.getNoteNumber()==64)check(e.sample>=43200,"Hammer on other string cut held MPE voice");
    }
    check(releasedOriginal,"Poly hammer failed to release old acoustic anchor before new anchor");h.released();
}
void stateAndCapacity(){
    Harness h;set(*h.processor,"autoInput",0);set(*h.processor,"mpeOutput",1);
    juce::MemoryBlock saved;h.processor->getStateInformation(saved);
    set(*h.processor,"autoInput",1);set(*h.processor,"mpeOutput",0);h.processor->setStateInformation(saved.getData(),int(saved.getSize()));
    check(get(*h.processor,"autoInput")==0&&get(*h.processor,"mpeOutput")==1,"New expression/input settings did not round trip");
    auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));check(xml!=nullptr,"Cannot parse state for backwards compatibility test");
    auto* values=xml->getChildByName("FLYSNAPPER_PARAMETERS");check(values!=nullptr,"Saved parameter state missing");
    for(int i=values->getNumChildElements();--i>=0;){auto* child=values->getChildElement(i);const auto id=child->getStringAttribute("id");if(id=="autoInput"||id=="mpeOutput")values->removeChildElement(child,true);}
    juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*xml,old);h.processor->setStateInformation(old.getData(),int(old.getSize()));
    check(get(*h.processor,"autoInput")==1&&get(*h.processor,"mpeOutput")==0,"Old preset failed safe default: automaticinput on, MPE off");
    set(*h.processor,"mpeOutput",1);std::vector<Raw> notes;for(int n=60;n<68;++n)notes.push_back({0,on(n)});h.block(128,silence,notes);
    std::vector<Raw> flood;for(int i=0;i<1900;++i)flood.push_back({128,juce::MidiMessage::pitchWheel(1,i%2?9000:8000)});h.block(128,silence,flood);
    check(h.processor->capacityFault()&&h.processor->droppedEvents()>0,"Expanded MPE event overflow was not surfaced");h.released();
    h.processor->requestPanic();h.block(128);
    h.block(128,silence,{{384,on(72)},{420,off(72)}});check(!h.processor->capacityFault(),"Explicit panic did not recover MPE allocator capacity fault");h.released();
}
}
int main(){try{
    juce::ScopedJuceInitialiser_GUI runtime;
    int failures=0;
    const auto run=[&](const char* name,void (*test)()){
        try{test();std::cout<<"PASS "<<name<<'\n';}
        catch(const std::exception& e){++failures;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}
    };
    run("chord isolation and output-mode lifecycle",chordIsolationAndModeChanges);
    run("held input controls and bypass",heldExternalStateAndBypass);
    run("actual mono audio and source shutdown",actualMonoAudioAndSourceShutdown);
    run("actual poly hammer and held string",actualPolyHammerKeepsOtherString);
    run("state compatibility and bounded output",stateAndCapacity);
    std::cout<<"FlysnapperMpeProcessor: "<<(failures?"FAIL":"PASS")<<" ("<<checks<<" checks, "<<failures<<" failed groups; actual JUCE MPE receiver; not Live/driver proof).\n";return failures?1:0;
}catch(const std::exception& e){std::cerr<<"FlysnapperMpeProcessor: FAIL: "<<e.what()<<'\n';return 1;}}
