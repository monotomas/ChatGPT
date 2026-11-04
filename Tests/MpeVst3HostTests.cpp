// SPDX-License-Identifier: AGPL-3.0-or-later
// Load the actual VST3 binary, including JUCE's MIDI event serialization, then
// deliver its output to an independent JUCE MPE instrument. No DAW or audio
// device is involved; this cannot establish Ableton/driver behavior.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks=0;
void check(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
void set(juce::AudioPluginInstance& p,const char* name,float normalised){
    for(auto* parameter:p.getParameters())if(parameter->getName(128)==name){parameter->setValueNotifyingHost(normalised);return;}
    throw std::runtime_error(std::string("Missing compiled VST3 parameter ")+name);
}
constexpr double rate=48000,twoPi=6.28318530717958647692;
struct Raw {int64_t sample;juce::MidiMessage midi;};
struct Session {
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::MPEInstrument receiver;
    std::function<float(int64_t)> signal;
    std::vector<Raw> events;
    int64_t position=0;
    Session(juce::AudioPluginFormatManager& formats,const juce::PluginDescription& description,bool audio=false){
        juce::String error;plugin=formats.createPluginInstance(description,rate,128,error);
        check(plugin!=nullptr,"Compiled MPE plugin could not load: "+error.toStdString());
        check(plugin->acceptsMidi()&&plugin->producesMidi(),"Compiled MPE plugin lost MIDI buses");
        plugin->releaseResources();
        check(plugin->getBusCount(true)==1&&plugin->getBus(true,0)->enable(audio),"Compiled MPE sidechain activation rejected");
        set(*plugin,"MPE Output",1);set(*plugin,"Correction",0);set(*plugin,"Pitch Enabled",audio?1.f:0.f);set(*plugin,"Mono Bend",0);
        plugin->prepareToPlay(rate,128);
    }
    ~Session(){plugin->releaseResources();}
    void process(juce::MidiBuffer& midi){
        juce::AudioBuffer<float> audio(std::max(2,std::max(plugin->getTotalNumInputChannels(),plugin->getTotalNumOutputChannels())),128);audio.clear();
        if(signal)for(int ch=0;ch<plugin->getTotalNumInputChannels();++ch)for(int n=0;n<128;++n)audio.setSample(ch,n,signal(position+n));
        plugin->processBlock(audio,midi);check(midi.getNumEvents()<=2048,"Compiled MPE plugin exceeded VST3 callback capacity");
        for(int ch=0;ch<plugin->getTotalNumOutputChannels();++ch)check(audio.getMagnitude(ch,0,128)==0,"Compiled MPE instrument leaked audio");
        for(const auto e:midi){const auto m=e.getMessage();check(e.samplePosition>=0&&e.samplePosition<128,"Compiled MPE timestamp outside block");
            if(m.isNoteOnOrOff())check(m.getChannel()>=2&&m.getChannel()<=15,"Compiled MPE emitted note on master/chord channel");
            if(m.isPitchWheel()&&m.getChannel()==1)check(m.getPitchWheelValue()==8192,"Compiled MPE forwarded external wheel to global master");
            if(m.isNoteOff())check(receiver.getNote(m.getChannel(),m.getNoteNumber()).isValid(),"Compiled MPE emitted unmatched receiver Note Off");
            if(m.isNoteOn())check(!receiver.getNote(m.getChannel(),m.getNoteNumber()).isValid(),"Compiled MPE reused member before release");
            receiver.processNextMidiEvent(m);events.push_back({position+e.samplePosition,m});
        }
        position+=128;
    }
    double pitch(int member,int anchor){const auto note=receiver.getNote(member,anchor);check(note.isValid(),"Compiled MPE receiver voice absent");return 69+12*std::log2(note.getFrequencyInHertz()/440.);}
    void expect(int member,int anchor,double target,double tolerance=.007){check(std::abs(pitch(member,anchor)-target)<tolerance,"Compiled VST3 MPE receiver pitch mismatch");}
};
juce::MidiMessage on(int channel,int note){return juce::MidiMessage::noteOn(channel,note,juce::uint8(93));}
juce::MidiMessage off(int channel,int note){return juce::MidiMessage::noteOff(channel,note);}
void compiledProtocol(juce::AudioPluginFormatManager& formats,const juce::PluginDescription& description){
    Session s(formats,description);juce::MidiBuffer midi;
    midi.addEvent(on(3,60),7);midi.addEvent(on(4,67),19);midi.addEvent(on(16,48),0);s.process(midi);
    const auto zone=s.receiver.getZoneLayout().getLowerZone();check(zone.numMemberChannels==14&&zone.perNotePitchbendRange==48&&zone.masterPitchbendRange==0,"Compiled RPN/MCM output was not reconstructed as fourteen-member MPE zone");
    s.expect(8,60,60);s.expect(9,67,67);
    bool first=false,second=false;for(const auto& e:s.events)if(e.midi.isNoteOn()){
        if(e.midi.getChannel()==8)first=e.sample==7;
        if(e.midi.getChannel()==9)second=e.sample==19;
    }
    check(first&&second,"Compiled event wrapper changed independent note timestamps");
    midi.clear();midi.addEvent(juce::MidiMessage::pitchWheel(3,9216),11);s.process(midi);s.expect(8,60,66);s.expect(9,67,67);
    midi.clear();midi.addEvent(juce::MidiMessage::pitchWheel(1,12288),17);s.process(midi);s.expect(8,60,67);s.expect(9,67,68);
    midi.clear();midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),7);s.process(midi);
    midi.clear();midi.addEvent(off(3,60),9);midi.addEvent(off(4,67),21);s.process(midi);check(s.receiver.getNumPlayingNotes()==2,"Compiled input MPE master sustain was lost");
    midi.clear();midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),7);s.process(midi);check(s.receiver.getNumPlayingNotes()==0,"Compiled input MPE master pedal release left notes");
}
std::vector<float> monoSignal(){
    std::vector<float> wave(43200);double phase=0;
    for(size_t n=0;n<wave.size();++n){const double t=n/rate,pitch=57+1.5*std::clamp((t-.18)/.35,0.,1.);phase+=twoPi*440*std::pow(2.,(pitch-69)/12.)/rate;
        wave[n]=float(.23*std::min(1.,t/.002)*std::exp(-.4*t)*(std::sin(phase)+.23*std::sin(2*phase+.2)+.09*std::sin(3*phase+.6)));
    }
    return wave;
}
void compiledMonoAudio(juce::AudioPluginFormatManager& formats,const juce::PluginDescription& description){
    Session s(formats,description,true);const auto wave=monoSignal();s.signal=[&](int64_t n){return n>=0&&size_t(n)<wave.size()?wave[size_t(n)]:0.f;};
    std::vector<double> errors;int settled=0;
    while(s.position<54000){const int64_t at=s.position;juce::MidiBuffer midi;
        if(at==0)midi.addEvent(on(1,84),0);
        if(at<=14400&&at+128>14400)midi.addEvent(juce::MidiMessage::pitchWheel(1,12288),int(14400-at));
        if(at<=45600&&at+128>45600)midi.addEvent(off(1,84),int(45600-at));
        s.process(midi);
        if(at>=28800&&at<38400){errors.push_back(std::abs(s.pitch(2,57)-58.5));s.expect(8,84,85);++settled;}
    }
    std::vector<Raw> genOn,genOff;for(const auto& e:s.events)if(e.midi.getChannel()==2){if(e.midi.isNoteOn())genOn.push_back(e);if(e.midi.isNoteOff())genOff.push_back(e);}
    check(genOn.size()==1&&genOff.size()==1&&genOn[0].midi.getNoteNumber()==57&&genOff[0].midi.getNoteNumber()==57,"Compiled MPE mono bend did not preserve one acoustic anchor lifecycle");
    check(genOn[0].sample<1920,"Compiled MPE mono acquisition exceeded40ms signal decision budget");check(settled>=15,"Compiled mono MPE receiver produced too few settled observations");
    std::sort(errors.begin(),errors.end());const double p95=errors[size_t(std::ceil(errors.size()*.95))-1];check(p95<.18,"Compiled MPE mono settled pitch error exceeded18cents");
    check(s.receiver.getNumPlayingNotes()==0,"Compiled MPE mono/source test left receiver held");
    std::cout<<"MEASURE actual VST3 MPE mono first decision "<<1000*genOn[0].sample/rate<<"ms; settled P95 "<<100*p95<<" cents; not driver latency\n";
}
void compiledPolyAudio(juce::AudioPluginFormatManager& formats,const juce::PluginDescription& description){
    Session s(formats,description,true);set(*s.plugin,"Pitch Mode",1);std::vector<float> wave(43200);double moving=0,fixed=0;
    for(size_t n=0;n<wave.size();++n){const double t=n/rate,pitch=57+2*std::clamp((t-.18)/.35,0.,1.);moving+=twoPi*440*std::pow(2.,(pitch-69)/12.)/rate;fixed+=twoPi*440*std::pow(2.,(64.-69)/12.)/rate;
        const auto string=[](double p){return std::sin(p)+.20*std::sin(2*p+.2)+.08*std::sin(3*p+.4);};
        wave[n]=float(std::min(1.,t/.002)*std::exp(-.18*t)*(.15*string(moving)+.11*string(fixed)));
    }
    s.signal=[&](int64_t n){return n>=0&&size_t(n)<wave.size()?wave[size_t(n)]:0.f;};
    int changing=-1,held=-1,observations=0;double maxChangingError=0,maxHeldError=0;
    while(s.position<55000){const int64_t at=s.position;juce::MidiBuffer midi;s.process(midi);
        for(const auto e:midi){const auto m=e.getMessage();if(m.isNoteOn()&&at+e.samplePosition<8640){if(m.getNoteNumber()==57)changing=m.getChannel();if(m.getNoteNumber()==64)held=m.getChannel();}}
        if(at>=28800&&at<38400){check(changing>=2&&changing<=7&&held>=2&&held<=7&&changing!=held,"Compiled poly tracker did not acquire independent source members");maxChangingError=std::max(maxChangingError,std::abs(s.pitch(changing,57)-59));maxHeldError=std::max(maxHeldError,std::abs(s.pitch(held,64)-64));++observations;}
    }
    check(observations>=15&&maxChangingError<.18&&maxHeldError<.18,"Compiled independently bent/held poly voices failed18cent settled receiver tolerance");
    for(const auto& e:s.events)if(e.midi.isNoteOn()&&e.midi.getChannel()==changing)check(e.midi.getNoteNumber()==57&&e.sample<8640,"Smooth poly bend was replaced with new anchor");
    check(s.receiver.getNumPlayingNotes()==0,"Compiled poly MPE test left receiver held");
    std::cout<<"MEASURE actual VST3 two-string MPE bend settled maxerror "<<100*maxChangingError<<" cents, heldstring "<<100*maxHeldError<<" cents\n";
}
int run(int argc,const juce::String& path){try{
    juce::ScopedJuceInitialiser_GUI gui;check(argc==2,"Expected complete Flysnapper.vst3 bundle path");juce::File bundle(path);check(bundle.isDirectory(),"Compiled VST3 bundle missing");
    juce::AudioPluginFormatManager formats;auto vst=std::make_unique<juce::VST3PluginFormat>();auto* format=vst.get();formats.addFormat(std::move(vst));juce::OwnedArray<juce::PluginDescription> descriptions;
    format->findAllTypesForFile(descriptions,bundle.getFullPathName());check(descriptions.size()==1,"Compiled MPE VST3 class discovery failed");int failures=0;
    const auto group=[&](const char* name,void (*test)(juce::AudioPluginFormatManager&,const juce::PluginDescription&)){
        try{test(formats,*descriptions[0]);std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failures;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}
    };
    group("actual VST3 MPE protocol serialization/master/member input",compiledProtocol);
    group("actual VST3 mono audio and independent keyboard expression",compiledMonoAudio);
    group("actual VST3 poly audio independently bent and held voices",compiledPolyAudio);
    std::cout<<"FlysnapperMpeVst3Host: "<<(failures?"FAIL":"PASS")<<" ("<<checks<<" checks, "<<failures<<" failed groups).\n";
    std::cout<<"Not verified: Ableton runtime, Windows unless run natively there, audio-driver latency, general musical readiness.\n";return failures?1:0;
}catch(const std::exception& e){std::cerr<<"FlysnapperMpeVst3Host: FAIL: "<<e.what()<<'\n';return 1;}}
}
#if JUCE_WINDOWS
int wmain(int argc,wchar_t** argv){return run(argc,argc==2?juce::String(argv[1]):juce::String());}
#else
int main(int argc,char** argv){return run(argc,argc==2?juce::String::fromUTF8(argv[1]):juce::String());}
#endif
