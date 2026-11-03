// SPDX-License-Identifier: AGPL-3.0-or-later
// Integration of actual sidechain samples, audio detection, note ownership and
// MIDI correction. This is a functional test, not an audio-driver latency test.
#include "PluginProcessor.h"
#include "PluginEditor.h"
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
void check(bool condition,const std::string& message) {
    ++checks;
    if(!condition)throw std::runtime_error(message);
}
void set(FlysnapperProcessor& processor,const char* id,float value) {
    auto* parameter=processor.parameters.getParameter(id);
    check(parameter!=nullptr,std::string("Missing audio integration parameter: ")+id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(FlysnapperProcessor& processor,const char* id) {
    auto* value=processor.parameters.getRawParameterValue(id);
    check(value!=nullptr,std::string("Missing audio integration parameter: ")+id);
    return value->load();
}
constexpr double pi=3.1415926535897932384626433832795;
double hz(int note){return 440.0*std::pow(2.0,(note-69)/12.0);}
using Signal=std::function<float(int64_t)>;
struct Input {int64_t sample;juce::MidiMessage message;};
struct Note {int64_t sample;bool on;int pitch,channel,velocity;};
bool operator==(const Note& a,const Note& b) {
    return a.sample==b.sample&&a.on==b.on&&a.pitch==b.pitch
        &&a.channel==b.channel&&a.velocity==b.velocity;
}
juce::MidiMessage on(int note,int channel=1,int velocity=100){return juce::MidiMessage::noteOn(channel,note,juce::uint8(velocity));}
juce::MidiMessage off(int note,int channel=1){return juce::MidiMessage::noteOff(channel,note);}
std::vector<Input> cChord(int64_t at=0){return {{at,on(48,16)},{at,on(52,16)},{at,on(55,16)}};}
Signal tone(double rate,int note,double seconds,double amplitude=.27,double start=0,bool harmonics=true) {
    return [=](int64_t sample){
        double time=double(sample)/rate-start;
        if(time<0||time>=seconds)return 0.0f;
        const double phase=2*pi*hz(note)*time;
        const double attack=std::min(1.0,time/.002);
        const double wave=std::sin(phase)+(harmonics?.30*std::sin(2*phase)+.12*std::sin(3*phase):0);
        return float(amplitude*attack*wave);
    };
}
Signal triad(double rate,double seconds) {
    return [=](int64_t sample){
        double time=double(sample)/rate;
        if(time<0||time>=seconds)return 0.0f;
        double wave=0;
        for(int note:{60,64,67}) {
            double phase=2*pi*hz(note)*time;
            wave+=.15*(std::sin(phase)+.20*std::sin(2*phase)+.08*std::sin(3*phase));
        }
        return float(std::min(1.0,time/.002)*wave);
    };
}
const Signal silence=[](int64_t){return 0.0f;};

class Harness {
public:
    explicit Harness(double sampleRate=48000,int maximumBlock=257,bool enableSidechain=true)
        : processor(std::make_unique<FlysnapperProcessor>()),rate(sampleRate) {
        auto layout=processor->getBusesLayout();
        layout.inputBuses.set(0,enableSidechain?juce::AudioChannelSet::stereo():juce::AudioChannelSet::disabled());
        check(processor->setBusesLayout(layout),"Cannot configure real sidechain bus");
        set(*processor,"pitchEnabled",1);set(*processor,"pitchMode",0);
        set(*processor,"inputGate",-55);set(*processor,"sensitivity",.65f);set(*processor,"releaseMs",35);
        set(*processor,"correction",1);set(*processor,"mode",2);set(*processor,"root",0);set(*processor,"scale",0);
        processor->prepareToPlay(rate,maximumBlock);
    }
    void sidechain(bool enabled) {
        auto layout=processor->getBusesLayout();
        layout.inputBuses.set(0,enabled?juce::AudioChannelSet::stereo():juce::AudioChannelSet::disabled());
        check(processor->setBusesLayout(layout),"Sidechain transition rejected");
    }
    void block(int count,const Signal& signal,const std::vector<Input>& input={},bool bypass=false) {
        juce::AudioBuffer<float> audio(2,count);
        juce::MidiBuffer midi;
        midi.ensureSize(131072);
        for(int n=0;n<count;++n) {
            const float sample=signal(position+n);
            audio.setSample(0,n,sample);audio.setSample(1,n,sample);
        }
        for(const auto& e:input)if(e.sample>=position&&e.sample<position+count)
            midi.addEvent(e.message,int(e.sample-position));
        if(bypass)processor->processBlockBypassed(audio,midi);else processor->processBlock(audio,midi);
        check(audio.getMagnitude(0,count)==0,"Flysnapper leaked sidechain audio to its silent instrument output");
        for(const auto metadata:midi) {
            check(metadata.samplePosition>=0&&metadata.samplePosition<count,"Audio-generated MIDI offset outside delivered block");
            const auto message=metadata.getMessage();
            if(!message.isNoteOnOrOff())continue;
            const bool isOn=message.isNoteOn();
            const int channel=message.getChannel(),pitch=message.getNoteNumber();
            check(channel>=1&&channel<=16&&pitch>=0&&pitch<128,"Invalid generated MIDI note");
            if(isOn) {
                check(message.getVelocity()>0,"Generated Note On has zero velocity");
                ++active[size_t(channel-1)][size_t(pitch)];
            }else {
                check(active[size_t(channel-1)][size_t(pitch)]>0,"Audio path emitted an unmatched output Note Off");
                --active[size_t(channel-1)][size_t(pitch)];
            }
            notes.push_back({position+metadata.samplePosition,isOn,pitch,channel,message.getVelocity()});
        }
        position+=count;
    }
    void run(int64_t frames,const Signal& signal,const std::vector<Input>& input={},
             const std::vector<int>& pattern={128},bool bypass=false) {
        size_t index=0;
        while(frames>0) {
            int n=int(std::min<int64_t>(frames,pattern[index++%pattern.size()]));
            block(n,signal,input,bypass);frames-=n;
        }
    }
    void allReleased(const std::string& label)const {
        for(const auto& channel:active)for(int voices:channel)
            check(voices==0,label+": a generated/external note was left active");
    }
    int sounding(int note,int channel=1)const{return active[size_t(channel-1)][size_t(note)];}
    std::vector<Note> ons()const {
        std::vector<Note> out;for(const auto& event:notes)if(event.on)out.push_back(event);return out;
    }
    std::unique_ptr<FlysnapperProcessor> processor;
    double rate;int64_t position=0;
    std::vector<Note> notes;
    std::array<std::array<int,128>,16> active{};
};

void disabledAndSilentInput() {
    Harness h(48000,257,false);
    h.run(24000,tone(h.rate,57,1),{}, {17,64,257});
    check(h.notes.empty(),"Unassigned/disabled sidechain produced audio-derived notes");
    check(!h.processor->sidechainActive(),"Disabled sidechain status is incorrect");
    h.sidechain(true);
    h.run(12000,silence);
    check(h.processor->sidechainActive(),"Digital silence was mistaken for disconnected sidechain");
    check(h.notes.empty(),"Silent connected input produced notes");
    set(*h.processor,"pitchEnabled",0);
    h.run(12000,tone(h.rate,57,2));
    check(h.notes.empty(),"Disabled Pitch-to-MIDI produced notes");
    // External MIDI remains usable with the audio module off.
    auto at=h.position;
    h.run(128,silence,{{at+7,on(61)},{at+93,off(61)}});
    check(h.notes.size()==2&&h.notes[0].sample==at+7&&h.notes[0].pitch==61
        &&!h.notes[1].on&&h.notes[1].sample==at+93,"Disabling audio module broke ordinary MIDI correction path");
    h.allReleased("disabled module");
    std::cout<<"PASS connected silence, disabled sidechain and disabled audio module; external MIDI preserved\n";
}

void monoRatesAndVariableBlocks() {
    for(double rate:{44100.,48000.,96000.})for(int block:{17,64,257}) {
        Harness h(rate,block);
        const int note=57;
        h.run(int64_t(rate*.40),tone(rate,note,.35),{}, {block});
        h.run(int64_t(rate*.25),silence,{}, {block});
        const auto ons=h.ons();
        check(!ons.empty(),"No mono audio-to-MIDI output in actual processor");
        for(const auto& event:ons)check(event.pitch==note&&event.channel==1,
            "Mono sidechain produced wrong pitch, octave or output channel: "+std::to_string(event.pitch));
        check(ons.size()==1,"Unbroken mono tone spuriously retriggered");
        check(ons.front().sample<=int64_t(rate*.15),"Mono functional acquisition exceeded 150 ms integration budget");
        h.allReleased("mono signal release");
        std::cout<<"MEASURE integrated mono first useful note "<<1000.*ons.front().sample/rate
                 <<" ms, "<<rate<<" Hz / "<<block<<" samples (not driver/end-to-end latency)\n";
    }
}

void correctionAtDecisionSample() {
    constexpr double rate=48000;
    auto fixture=tone(rate,61,.30);
    Harness baseline(rate,257);set(*baseline.processor,"mode",0);
    baseline.run(24000,fixture,cChord(),{17,64,257,128});
    auto events=baseline.ons();check(events.size()==1&&events[0].pitch==60,
        "Audio detection did not enter the common Chord Notes correction engine");
    const int64_t decision=events[0].sample;
    auto replay=[&](int64_t changeSample){
        Harness h(rate,257);set(*h.processor,"mode",0);
        auto controls=cChord();
        for(int note:{48,52,55})controls.push_back({changeSample,off(note,16)});
        for(int note:{50,53,57})controls.push_back({changeSample,on(note,16)});
        h.run(24000,fixture,controls,{17,64,257,128});h.allReleased("sample-timed chord correction");
        return h.ons();
    };
    const auto same=replay(decision),later=replay(decision+1);
    check(same.size()==1&&same[0].sample==decision&&same[0].pitch==62,
        "Chord at exact generated decision sample was not applied first");
    check(later.size()==1&&later[0].sample==decision&&later[0].pitch==60,
        "A future chord sample incorrectly changed an earlier generated note");
    baseline.allReleased("baseline chord correction");
    std::cout<<"PASS actual audio decisions use same-sample chord first and never a future chord; original Note Off remains bound\n";
}

void concurrentExternalOwnership() {
    Harness h;
    auto fixture=tone(h.rate,60,2);
    h.run(12000,fixture,{{0,on(60)}});
    check(h.sounding(60)==1,"External/audio shared note did not remain active");
    check(h.ons().size()==1&&h.ons()[0].sample==0,"Audio retriggered an externally held output collision");
    check((h.processor->getDetectedMask()&1u)!=0,"Audio source never acquired colliding C");
    check(std::isfinite(h.processor->getInputLevelDb())&&h.processor->getInputLevelDb()>-55
        &&h.processor->getInputLevelDb()<=0,"Actual input meter did not track finite above-gate sidechain audio");
    set(*h.processor,"pitchEnabled",0);
    h.run(12000,fixture);
    check(h.sounding(60)==1,"Disabling detected source cut an external held note");
    set(*h.processor,"pitchEnabled",1);
    h.run(12000,fixture);
    check((h.processor->getDetectedMask()&1u)!=0,"Re-enabled audio source failed to reacquire current input");
    const int64_t externalOff=h.position+13;
    h.run(128,fixture,{{externalOff,off(60)}});
    check(h.sounding(60)==1,"External Note Off cut independently held generated source");
    const int64_t generatedOff=h.position;
    set(*h.processor,"pitchEnabled",0);h.block(128,fixture);
    check(h.notes.back().sample==generatedOff&&!h.notes.back().on&&h.notes.back().pitch==60,
        "Disabling last generated owner failed to end its output at the current block start");
    h.allReleased("source-only shutdown");
    check(h.ons().size()==1,"Enable/disable of shared source retriggered common output");
    std::cout<<"PASS real detected and external MIDI coexist, share correction and release only their own note ownership\n";
}

void polyphonyAndModeTransition() {
    Harness h;
    set(*h.processor,"pitchMode",1);
    h.run(28800,triad(h.rate,.50),{}, {17,64,257,128});
    h.run(14400,silence,{}, {17,64,257,128});
    std::array<bool,128> heard{};
    for(const auto& event:h.ons()) {
        check(event.pitch==60||event.pitch==64||event.pitch==67,
            "Polyphonic processor emitted an unrelated pitch or harmonic: "+std::to_string(event.pitch));
        check(event.channel==1,"Generated polyphonic note left performance channel 1");
        heard[size_t(event.pitch)]=true;
    }
    check(heard[60]&&heard[64]&&heard[67],"Actual polyphonic audio path missed a C/E/G voice");
    h.allReleased("polyphonic triad");
    // Switch modes while a generated voice and unrelated external voice coexist.
    auto mono=tone(h.rate,57,3);
    set(*h.processor,"pitchMode",0);
    h.run(12000,mono,{{h.position,on(72)}});
    check(h.sounding(72)==1,"Mode transition lost external input fixture");
    check(h.sounding(57)==1,"Mono mode after poly failed to detect current input");
    set(*h.processor,"pitchMode",1);h.block(128,silence);
    check(h.sounding(57)==0,"Mode transition retained stale generated voice");
    check(h.sounding(72)==1,"Mode transition terminated external voice");
    const auto at=h.position;
    h.run(12000,silence,{{at+11,off(72)}});
    h.allReleased("poly/mono mode transition");
    std::cout<<"PASS actual polyphonic sidechain triad and mono/poly transitions without external note interruption\n";
}

void stateAndEditorIndependence() {
    Harness a;
    set(*a.processor,"pitchMode",1);set(*a.processor,"inputGate",-61);
    set(*a.processor,"sensitivity",.73f);set(*a.processor,"releaseMs",48);set(*a.processor,"pitchEnabled",0);
    juce::MemoryBlock state;a.processor->getStateInformation(state);
    Harness b;b.processor->setStateInformation(state.getData(),int(state.getSize()));
    for(const char* id:{"pitchEnabled","pitchMode","inputGate","sensitivity","releaseMs"})
        check(std::abs(get(*a.processor,id)-get(*b.processor,id))<.00001f,std::string("Audio state did not restore: ")+id);
    b.run(12000,tone(b.rate,57,.20));check(b.notes.empty(),"State restore unexpectedly armed audio source");
    // Identical samples/parameter values must give identical generated MIDI
    // whether the editor is closed or showing its mapping table.
    auto fixture=tone(48000,57,.30);
    auto render=[&](bool editorOpen){
        Harness h;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        if(editorOpen) {
            editor.reset(h.processor->createEditor());
            std::function<juce::Component*(juce::Component&)> findFlip;
            findFlip=[&](juce::Component& component)->juce::Component* {
                if(component.getComponentID()=="flip")return &component;
                for(int i=0;i<component.getNumChildComponents();++i)
                    if(auto* found=findFlip(*component.getChildComponent(i)))return found;
                return nullptr;
            };
            auto* flip=dynamic_cast<juce::TextButton*>(findFlip(*editor));
            check(flip!=nullptr,"Actual editor no longer exposes its view-only flip");
            flip->onClick();
        }
        h.run(24000,fixture,{}, {17,64,257,128});h.allReleased("editor independence");
        return h.notes;
    };
    auto closed=render(false),opened=render(true);
    check(!closed.empty(),"Editor-independence audio fixture produced no MIDI");
    check(closed==opened,"Opening editor changed generated note timing or content");
    std::cout<<"PASS audio settings state and identical audio-to-MIDI with editor closed/open\n";
}

void repeatedSilenceAndSourceCycles() {
    Harness h;
    for(int cycle=0;cycle<12;++cycle) {
        const auto origin=h.position;
        Signal fixture=[=](int64_t sample){
            double time=double(sample-origin)/48000.;
            if(time<0||time>=.18)return 0.0f;
            double phase=2*pi*220*time;
            return float(.30*std::min(1.0,time/.002)*(std::sin(phase)+.25*std::sin(2*phase)));
        };
        h.run(10000,fixture,{}, {17,64,257,128});
        if(cycle%3==0){h.sidechain(false);h.block(128,silence);h.sidechain(true);}
        else if(cycle%3==1){h.processor->requestPanic();h.block(128,silence);}
        else {h.run(7000,silence,{}, {17,64,257,128});}
        h.run(8000,silence,{}, {17,64,257,128});
        h.allReleased("repeated source/silence/panic lifecycle");
        check(!h.processor->capacityFault(),"Normal audio source cycles caused capacity fault");
    }
    check(h.ons().size()>=12,"Repeated real audio onsets were lost across source resets");
    std::cout<<"PASS twelve audio attack/silence/source-disconnect/panic cycles without stale output notes\n";
}
}

int main() {
    try {
        juce::ScopedJuceInitialiser_GUI gui;
        disabledAndSilentInput();monoRatesAndVariableBlocks();correctionAtDecisionSample();
        concurrentExternalOwnership();polyphonyAndModeTransition();stateAndEditorIndependence();
        repeatedSilenceAndSourceCycles();
        std::cout<<"FlysnapperProcessorAudio: PASS ("<<checks<<" checks). Functional integration budget is 150 ms; actual first-note timings are printed, not substituted for the requested live latency goals.\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"FlysnapperProcessorAudio: FAIL: "<<error.what()<<"\n";return 1;
    }
}
