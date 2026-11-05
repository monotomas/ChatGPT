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
void compiledPolyStaticAudio(juce::AudioPluginFormatManager& formats,const juce::PluginDescription& description){
    // November5 scope: polyphony emits discrete semitone notes, with no
    // continuous poly bend requirement. The obsolete November4 curve captures
    // remain under docs/validation/musical-readiness-research/native-checkpoint-v20.
    // This observable, harmonically rich two-source fixture tests the compiled
    // routing contract; it does not replace the retained sparse45+64 diagnostic
    // or establish general guitar transcription quality.
    Session s(formats,description,true);set(*s.plugin,"Pitch Mode",1);
    set(*s.plugin,"Mode",0);set(*s.plugin,"Correction",1);
    constexpr int64_t step=19200,lowerEnd=43200,upperEnd=55200,chordChange=31200,externalStart=36000,externalEnd=58560;
    std::vector<float> wave(size_t(upperEnd),0.f);double changingPhase=0,heldPhase=0;
    for(size_t n=0;n<wave.size();++n){
        const double t=n/rate,pitch=int64_t(n)<step?48.:60.;
        changingPhase+=twoPi*440*std::pow(2.,(pitch-69)/12.)/rate;
        heldPhase+=twoPi*440*std::pow(2.,(64.-69)/12.)/rate;
        double changing=0,held=0;
        for(int harmonic=1;harmonic<=12;++harmonic){
            const double amplitude=1/std::pow(double(harmonic),1.25);
            changing+=amplitude*std::sin(harmonic*changingPhase+.17*harmonic);
            held+=amplitude*std::sin(harmonic*heldPhase+.11*harmonic);
        }
        const double envelope=std::min(1.,t/.002)*std::exp(-.18*t);
        wave[n]=float(envelope*((int64_t(n)<lowerEnd?.15*changing:0)+.11*held));
    }
    s.signal=[&](int64_t n){return n>=0&&size_t(n)<wave.size()?wave[size_t(n)]:0.f;};
    int original=-1,held=-1,replacement=-1;int observations=0;
    while(s.position<74400){
        const int64_t at=s.position;juce::MidiBuffer midi;
        const auto add=[&](int64_t sample,const juce::MidiMessage& message){if(sample>=at&&sample<at+128)midi.addEvent(message,int(sample-at));};
        // Dminor chord tones correct source48->50,64->65 and60->62.
        for(int note:{50,53,57})add(0,on(16,note));
        // Eminor becomes the new context, but already sounding corrected
        // audio notes must retain their original output bindings.
        for(int note:{50,53,57})add(chordChange,off(16,note));
        for(int note:{52,55,59})add(chordChange,on(16,note));
        add(externalStart,on(1,74));add(externalEnd,off(1,74));
        for(int note:{52,55,59})add(upperEnd+256,off(16,note));
        s.process(midi);
        for(const auto event:midi){const auto m=event.getMessage();if(m.isNoteOn()&&m.getChannel()<=7){
            if(m.getNoteNumber()==50)original=m.getChannel();
            if(m.getNoteNumber()==65)held=m.getChannel();
            if(m.getNoteNumber()==62)replacement=m.getChannel();
        }}
        if(at>=34560&&at<40560){
            check(held>=2&&held<=7&&replacement>=2&&replacement<=7&&held!=replacement,"Compiled static poly sources did not occupy independent generated members");
            s.expect(held,65,65);s.expect(replacement,62,62);++observations;
        }
        if(at>=lowerEnd+4800&&at<upperEnd-128){
            s.expect(held,65,65);
            check(!s.receiver.getNote(replacement,62).isValid(),"Damped lower source remained sounding while upper source was held");
        }
        if(at>=externalStart+128&&at<externalEnd-128)s.expect(8,76,76);
    }
    std::vector<Raw> generatedOns,generatedOffs,externalOns,externalOffs;
    for(const auto& e:s.events){const int channel=e.midi.getChannel();
        if(channel>=2&&channel<=7){
            if(e.midi.isNoteOn())generatedOns.push_back(e);
            if(e.midi.isNoteOff())generatedOffs.push_back(e);
            if(e.midi.isPitchWheel())check(e.midi.getPitchWheelValue()==8192,"Static poly output generated a non-neutral pitch bend");
        }else if(channel>=8&&channel<=15){
            if(e.midi.isNoteOn())externalOns.push_back(e);
            if(e.midi.isNoteOff())externalOffs.push_back(e);
        }
    }
    check(original>=2&&original<=7&&held>=2&&held<=7&&original!=held,"Compiled static poly failed to acquire both original semitone notes");
    check(observations>=15,"Compiled static poly receiver produced too few independent held observations");
    check(generatedOns.size()==3&&generatedOffs.size()==3,"Static poly produced extra/missing generated note lifecycles");
    int64_t firstOriginal=-1,firstHeld=-1,firstReplacement=-1,offOriginal=-1,offHeld=-1,offReplacement=-1;
    for(const auto& e:generatedOns){
        const int note=e.midi.getNoteNumber();
        check(e.sample>0,"Channel16 chord control leaked as an immediate playing note");
        if(note==50&&firstOriginal<0)firstOriginal=e.sample;
        else if(note==65&&firstHeld<0)firstHeld=e.sample;
        else if(note==62&&firstReplacement<0)firstReplacement=e.sample;
        else check(false,"Static poly emitted a duplicate or incorrectly corrected semitone");
    }
    for(const auto& e:generatedOffs){
        const int note=e.midi.getNoteNumber();
        if(note==50)offOriginal=e.sample;else if(note==65)offHeld=e.sample;else if(note==62)offReplacement=e.sample;
        else check(false,"Static poly released an unexpected corrected note");
    }
    check(firstOriginal>0&&firstOriginal<step&&firstHeld>0&&firstHeld<step,"Initial source notes were not acquired before the fret step");
    check(firstReplacement>=step&&firstReplacement<lowerEnd,"Discrete fret step did not produce a new corrected semitone");
    check(offOriginal>=step&&offOriginal<=step+4800,"Fret step released old corrected note prematurely or retained it beyond100ms");
    check(offReplacement>=lowerEnd&&offHeld>=upperEnd,"A source ended prematurely or damping one source cut the held other source");
    check(externalOns.size()==1&&externalOffs.size()==1&&externalOns[0].midi.getNoteNumber()==76&&externalOffs[0].midi.getNoteNumber()==76,"Updated chord control did not correct the independent external playing lifecycle");
    check(externalOns[0].sample==externalStart&&externalOffs[0].sample==externalEnd,"Static audio processing changed external MIDI event timestamps");
    check(s.receiver.getNumPlayingNotes()==0,"Compiled static poly test left receiver notes hanging");
    std::cout<<"MEASURE actual VST3 static poly first decisions "<<1000*firstOriginal/rate<<" / "<<1000*firstHeld/rate
        <<"ms; new semitone "<<1000*(firstReplacement-step)/rate<<"ms after fret step; old note release "<<1000*(offOriginal-step)/rate<<"ms; not driver latency\n";
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
    group("actual VST3 static poly semitones, chord correction and independent releases",compiledPolyStaticAudio);
    std::cout<<"FlysnapperMpeVst3Host: "<<(failures?"FAIL":"PASS")<<" ("<<checks<<" checks, "<<failures<<" failed groups).\n";
    std::cout<<"Not verified: Ableton runtime, Windows unless run natively there, audio-driver latency, general musical readiness.\n";return failures?1:0;
}catch(const std::exception& e){std::cerr<<"FlysnapperMpeVst3Host: FAIL: "<<e.what()<<'\n';return 1;}}
}
#if JUCE_WINDOWS
int wmain(int argc,wchar_t** argv){return run(argc,argc==2?juce::String(argv[1]):juce::String());}
#else
int main(int argc,char** argv){return run(argc,argc==2?juce::String::fromUTF8(argv[1]):juce::String());}
#endif
