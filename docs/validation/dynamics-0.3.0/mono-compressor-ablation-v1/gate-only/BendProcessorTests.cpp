// SPDX-License-Identifier: AGPL-3.0-or-later
// Genuine sidechain samples through the processor, correction and MIDI adapter.
// These tests establish MIDI expression behavior, not Live/driver latency.
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks=0;
void check(bool condition,const std::string& message) {
    ++checks;if(!condition)throw std::runtime_error(message);
}
void set(FlysnapperProcessor& processor,const char* id,float value) {
    auto* parameter=processor.parameters.getParameter(id);
    check(parameter!=nullptr,std::string("Missing bend parameter: ")+id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(FlysnapperProcessor& processor,const char* id) {
    auto* value=processor.parameters.getRawParameterValue(id);
    check(value!=nullptr,std::string("Missing bend value: ")+id);return value->load();
}
constexpr double pi=3.14159265358979323846;
using Signal=std::vector<float>;
struct Raw {int64_t sample;juce::MidiMessage midi;};

Signal readWave(const std::filesystem::path& file) {
    std::ifstream stream(file,std::ios::binary);check(bool(stream),"Cannot open sealed bend PCM fixture");
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)),{});
    const auto u16=[&](size_t at){check(at+2<=bytes.size(),"Truncated bend WAV");return uint16_t(bytes[at])|(uint16_t(bytes[at+1])<<8);};
    const auto u32=[&](size_t at){return uint32_t(u16(at))|(uint32_t(u16(at+2))<<16);};
    check(bytes.size()>=12&&std::string(reinterpret_cast<const char*>(bytes.data()),4)=="RIFF"
        &&std::string(reinterpret_cast<const char*>(bytes.data()+8),4)=="WAVE","Not a RIFF bend WAV");
    size_t data=0,length=0;int format=0,channels=0,rate=0,bits=0;
    for(size_t at=12;at+8<=bytes.size();) {
        const uint32_t count=u32(at+4);check(at+8+uint64_t(count)<=bytes.size(),"Bend WAV chunk outside file");
        const std::string name(reinterpret_cast<const char*>(bytes.data()+at),4);
        if(name=="fmt ") {
            check(count>=16,"Truncated bend WAV format");format=u16(at+8);channels=u16(at+10);rate=int(u32(at+12));bits=u16(at+22);
        }else if(name=="data"){data=at+8;length=count;}
        at+=8+count+(count&1);
    }
    check(format==1&&channels==1&&rate==48000&&bits==16&&data>0&&(length&1)==0,"Sealed bend fixture must be 48kHz mono PCM16");
    Signal wave(length/2);for(size_t i=0;i<wave.size();++i)wave[i]=float(int16_t(u16(data+2*i)))/32768.f;return wave;
}
struct Truth {int64_t sample;double pitch;};
std::vector<Truth> readCurve(const std::filesystem::path& file,const std::string& name) {
    std::ifstream stream(file);check(bool(stream),"Cannot open sealed bend pitch truth");
    std::string line;check(bool(std::getline(stream,line))&&line=="case\tsample\tpitch","Unexpected sealed pitch truth header");
    std::vector<Truth> truth;
    while(std::getline(stream,line)) {
        std::istringstream row(line);std::string label,sample,pitch;
        check(bool(std::getline(row,label,'\t'))&&bool(std::getline(row,sample,'\t'))&&bool(std::getline(row,pitch)),"Invalid sealed pitch truth row");
        if(label!=name)continue;
        const Truth point{std::stoll(sample),std::stod(pitch)};
        check(point.sample>=2400&&std::isfinite(point.pitch)&&point.pitch>50&&point.pitch<70,"Invalid sealed pitch reference");
        if(!truth.empty())check(point.sample-truth.back().sample==64,"Sealed pitch truth lost canonical 64-sample grid");
        truth.push_back(point);
    }
    check(truth.size()>=1000,"Sealed bend reference coverage is incomplete");return truth;
}

Signal bentGuitar(double rate,int base,double depth,double seconds=.9) {
    Signal wave(size_t(std::ceil(rate*(seconds+.25))),0);
    double phase=0;
    for(size_t n=0;n<wave.size();++n) {
        const double time=n/rate;if(time>=seconds)break;
        const double curve=std::clamp((time-.18)/.35,0.,1.);
        const double vibrato=time>.58?.045*std::sin(2*pi*5.3*(time-.58)):0;
        const double pitch=base+depth*curve+vibrato;
        const double frequency=440*std::pow(2.,(pitch-69)/12.);
        phase+=2*pi*frequency/rate;
        const double envelope=.28*std::min(1.,time/.002)*std::exp(-time*.45);
        wave[n]=float(envelope*(std::sin(phase)+.23*std::sin(2*phase+.2)+.09*std::sin(3*phase+.6)));
    }
    return wave;
}

class Harness {
public:
    explicit Harness(double sampleRate=48000,int maximumBlock=257)
        :processor(std::make_unique<FlysnapperProcessor>()),rate(sampleRate) {
        auto layout=processor->getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::stereo());
        check(processor->setBusesLayout(layout),"Bend fixture could not enable real sidechain");
        set(*processor,"pitchEnabled",1);set(*processor,"pitchMode",0);
        set(*processor,"monoBend",1);set(*processor,"bendRange",0);
        set(*processor,"inputGate",-55);set(*processor,"sensitivity",.65f);set(*processor,"releaseMs",35);
        set(*processor,"mode",2);set(*processor,"scale",0);set(*processor,"correction",1);set(*processor,"compressorThresholdDb",0);set(*processor,"compressorRatio",1);set(*processor,"limiterCeilingDb",0);
        processor->prepareToPlay(rate,maximumBlock);
    }
    void block(int count,const Signal& signal,const std::vector<Raw>& input={},bool bypass=false) {
        juce::AudioBuffer<float> audio(2,count);juce::MidiBuffer midi;
        for(int n=0;n<count;++n) {
            const auto at=position+n;const float value=at>=0&&at<int64_t(signal.size())?signal[size_t(at)]:0;
            audio.setSample(0,n,value);audio.setSample(1,n,value);
        }
        for(const auto& e:input)if(e.sample>=position&&e.sample<position+count)midi.addEvent(e.midi,int(e.sample-position));
        if(bypass)processor->processBlockBypassed(audio,midi);else processor->processBlock(audio,midi);
        for(int channel=0;channel<audio.getNumChannels();++channel)
            if(count>0)check(audio.getMagnitude(channel,0,count)==0,"Bend path leaked input audio");
        for(const auto event:midi) {
            check(event.samplePosition>=0&&(count==0?event.samplePosition==0:event.samplePosition<count),"Bend event outside actual process block");
            const auto message=event.getMessage();const int channel=message.getChannel();
            check(channel>=1&&channel<=16,"Invalid bend event channel");
            if(message.isNoteOnOrOff()) {
                auto& voice=active[size_t(channel-1)][size_t(message.getNoteNumber())];
                if(message.isNoteOn())++voice;else {check(voice>0,"Unmatched bend-path Note Off");--voice;}
            }
            if(message.isPitchWheel())check(message.getPitchWheelValue()>=0&&message.getPitchWheelValue()<=16383,"Invalid 14-bit pitchwheel");
            events.push_back({position+event.samplePosition,message});
        }
        position+=count;
    }
    void run(int64_t frames,const Signal& signal,const std::vector<Raw>& input={},const std::vector<int>& pattern={128}) {
        size_t i=0;while(frames>0){const int n=int(std::min<int64_t>(frames,pattern[i++%pattern.size()]));block(n,signal,input);frames-=n;}
    }
    std::vector<Raw> notes(int channel,bool on)const {
        std::vector<Raw> found;for(const auto& e:events)
            if(e.midi.getChannel()==channel&&(on?e.midi.isNoteOn():e.midi.isNoteOff()))found.push_back(e);
        return found;
    }
    void allReleased()const {for(const auto& ch:active)for(int n:ch)check(n==0,"Bend fixture left a held note");}
    bool sounding(int channel,int note)const{return active[size_t(channel-1)][size_t(note)]>0;}
    std::unique_ptr<FlysnapperProcessor> processor;
    double rate;int64_t position=0;std::vector<Raw> events;
    std::array<std::array<int,128>,16> active{};
};

void requireRangeBeforeOn(const Harness& h,int range) {
    const auto first=std::find_if(h.events.begin(),h.events.end(),[](const Raw& e){return e.midi.getChannel()==2&&e.midi.isNoteOn();});
    check(first!=h.events.end(),"No bent note reached the output adapter");
    std::vector<std::array<int,2>> controls;bool wheel=false;
    for(auto i=h.events.begin();i!=first;++i)if(i->midi.getChannel()==2) {
        if(i->midi.isController())controls.push_back({i->midi.getControllerNumber(),i->midi.getControllerValue()});
        if(i->midi.isPitchWheel())wheel=true;
    }
    const std::vector<std::array<int,2>> expected{{101,0},{100,0},{6,range},{38,0},{101,127},{100,127}};
    check(controls==expected,"Standard RPN pitch-bend sensitivity must precede first generated Note On");
    check(wheel,"Initial wheel must precede first generated Note On");
}
void requireCentreAfterOff(const Harness& h) {
    const auto off=h.notes(2,false);check(!off.empty(),"Missing bent Note Off");
    const auto finalOff=off.back().sample;bool centred=false;
    for(const auto& e:h.events)if(e.sample>=finalOff&&e.midi.getChannel()==2&&e.midi.isPitchWheel())
        centred=e.midi.getPitchWheelValue()==8192;
    check(centred,"Generated bend did not centre after its final Note Off");
    // Equal-sample order matters: the note is released before the centre.
    const auto lastOff=std::find_if(h.events.rbegin(),h.events.rend(),[](const Raw& e){return e.midi.getChannel()==2&&e.midi.isNoteOff();});
    bool after=false;
    for(auto i=lastOff.base();i!=h.events.end();++i)
        if(i->midi.getChannel()==2&&i->midi.isPitchWheel()&&i->midi.getPitchWheelValue()==8192){after=true;break;}
    check(after,"Bend centre was delivered before the last generated Note Off");
}

void continuousBendAndExternalExpression() {
    for(double rate:{44100.,48000.,96000.})for(int range:{2,12}) {
        Harness h(rate);set(*h.processor,"bendRange",range==12?1.f:0.f);
        const double depth=range==2?2.:5.;const auto wave=bentGuitar(rate,57,depth);
        const std::vector<Raw> input{{int64_t(rate*.10),juce::MidiMessage::noteOn(1,72,juce::uint8(91))},
            {int64_t(rate*.20),juce::MidiMessage::pitchWheel(1,10000)},
            {int64_t(rate*.96),juce::MidiMessage::noteOff(1,72)}};
        h.run(int64_t(rate*1.12),wave,input,{17,64,257,128});
        const auto on=h.notes(2,true),off=h.notes(2,false);
        check(on.size()==1&&off.size()==1,"Continuous bend retriggered/replaced the monophonic anchor");
        check(on[0].midi.getNoteNumber()==57&&off[0].midi.getNoteNumber()==57,"Bend Note Off lost its original anchor");
        requireRangeBeforeOn(h,range);requireCentreAfterOff(h);h.allReleased();
        int maximum=8192,wheels=0,settled=0;bool externalWheel=false;
        for(const auto& e:h.events)if(e.midi.isPitchWheel()) {
            if(e.midi.getChannel()==2){
                maximum=std::max(maximum,e.midi.getPitchWheelValue());++wheels;
                const double time=e.sample/rate;
                if(time>.65&&time<.88) {
                    const double reconstructed=57+double(e.midi.getPitchWheelValue()-8192)*range/8192.;
                    const double expected=57+depth+.045*std::sin(2*pi*5.3*(time-.58));
                    check(std::abs(reconstructed-expected)<.18,"Settled audio bend differs from acoustic truth by >=18 cents");++settled;
                }
                if(time>=.88&&time<off[0].sample/rate&&e.midi.getPitchWheelValue()!=8192) {
                    const double reconstructed=57+double(e.midi.getPitchWheelValue()-8192)*range/8192.;
                    check(std::abs(reconstructed-(57+depth))<.30,"Release manufactured a spurious expressive pitch before Note Off");
                }
            }
            else if(e.midi.getChannel()==1&&e.sample==input[1].sample&&e.midi.getPitchWheelValue()==10000)externalWheel=true;
        }
        check(wheels>=8,"Continuous audio curve produced too few expression updates");
        check(settled>=8,"Too few settled pitch updates to verify acoustic bend accuracy");
        check(maximum>(range==2?13500:11000),"Generated wheel failed to follow a real multi-semitone acoustic bend");
        check(externalWheel,"Generated expression modified/dropped external channel-1 wheel");
        const auto extOn=h.notes(1,true),extOff=h.notes(1,false);
        check(extOn.size()==1&&extOff.size()==1&&extOn[0].sample==input[0].sample&&extOff[0].sample==input[2].sample
            &&extOn[0].midi.getNoteNumber()==72&&extOn[0].midi.getVelocity()==91,"Bend mode altered unrelated external channel-1 notes");
        std::cout<<"MEASURE mono bend "<<range<<" semitone range, "<<rate<<" Hz: "<<wheels<<" wheel events, maximum "<<maximum<<"; fixed note57/ch2\n";
    }
}

void correctedAnchorKeepsExpression() {
    Harness h;set(*h.processor,"scale",1); // C major: C# goes to D.
    const auto wave=bentGuitar(h.rate,61,1.5);h.run(54000,wave);
    const auto on=h.notes(2,true),off=h.notes(2,false);
    check(on.size()==1&&off.size()==1&&on[0].midi.getNoteNumber()==62&&off[0].midi.getNoteNumber()==62,
          "Correction did not retain the bent note's original corrected binding");
    int maximum=8192;for(const auto& e:h.events)if(e.midi.getChannel()==2&&e.midi.isPitchWheel())maximum=std::max(maximum,e.midi.getPitchWheelValue());
    check(maximum>13500,"Correction erased expressive bend around the corrected anchor");
    requireCentreAfterOff(h);h.allReleased();
}

void detunedRepicksRestoreWheelBeforeEveryOn() {
    for(double rate:{44100.,48000.,96000.})for(int range:{2,12}) {
        Harness h(rate);set(*h.processor,"bendRange",range==12?1.f:0.f);
        Signal wave(size_t(rate*1.3),0);
        const double frequency=440*std::pow(2.,(57.3-69)/12.);
        for(size_t n=0;n<wave.size();++n) {
            const double time=n/rate;if(time>=1.05)break;
            double value=0;
            // Three genuine excitations overlap the previous ringing tail.
            // There is no silence gap, and all three share the same +30-cent
            // acoustic pitch, so deduplicating expression across Note Off is
            // observably wrong after the adapter has centred the wheel.
            for(int pick=0;pick<3;++pick) {
                const double age=time-.32*pick;if(age<0)continue;
                const double phase=2*pi*frequency*age+1.6*pick;
                value+=.23*std::min(1.,age/.001)*std::exp(-6*age)
                    *(std::sin(phase)+.25*std::sin(2*phase+.2)+.09*std::sin(3*phase+.6));
            }
            wave[n]=float(value);
        }
        h.run(int64_t(wave.size()),wave,{}, {64,257,17,128});
        try {
        int wheel=8192,ons=0;
        const std::vector<std::array<int,2>> expectedRpn{{101,0},{100,0},{6,range},{38,0},{101,127},{100,127}};
        std::vector<std::array<int,2>> pendingRpn;
        for(const auto& e:h.events)if(e.midi.getChannel()==2) {
            if(e.midi.isController())pendingRpn.push_back({e.midi.getControllerNumber(),e.midi.getControllerValue()});
            if(e.midi.isPitchWheel())wheel=e.midi.getPitchWheelValue();
            if(e.midi.isNoteOn()) {
                ++ons;check(e.midi.getNoteNumber()==57,"Detuned repick changed its MIDI anchor");
                check(pendingRpn==expectedRpn,"Every new bent note must establish RPN range before Note On");
                pendingRpn.clear();
                const double fraction=double(wheel-8192)*range/8192.;
                check(std::abs(fraction-.3)<(ons==1?.18:.08),"A repeated detuned Note On inherited centred/stale expression");
            }
        }
        check(ons==3&&h.notes(2,false).size()==3,"Continuous ringing-string repicks were lost or duplicated in bend mode");
        requireRangeBeforeOn(h,range);requireCentreAfterOff(h);h.allReleased();
        } catch(...) {
            const std::string filename="Flysnapper-detuned-repick-failure-"+std::to_string(int(rate))+"-"+std::to_string(range)+".csv";
            std::ofstream dump(filename);dump<<"rate,range,sample,type,channel,data0,data1,data2\n";
            std::cerr<<"DIAGNOSTIC unchanged detuned repick rate="<<rate<<" range="<<range<<"; full MIDI decisions in "<<filename<<'\n';
            for(const auto& e:h.events){const auto& m=e.midi;const auto* b=m.getRawData();const int count=m.getRawDataSize();const char type=m.isNoteOn()?'N':m.isNoteOff()?'F':m.isPitchWheel()?'P':'C';
                dump<<int(rate)<<','<<range<<','<<e.sample<<','<<type<<','<<m.getChannel()<<','<<int(b[0])<<','<<(count>1?int(b[1]):0)<<','<<(count>2?int(b[2]):0)<<'\n';
                if(m.isNoteOnOrOff())std::cerr<<"  "<<e.sample<<' '<<type<<" note="<<m.getNoteNumber()<<" channel="<<m.getChannel()<<'\n';
            }
            throw;
        }
    }
}

void ignoredBendAutomationKeepsHeldNotes() {
    for(bool polyphonic:{false,true}) {
        Harness baseline,automated;
        for(auto* h:{&baseline,&automated}) {
            set(*h->processor,"pitchMode",polyphonic?1.f:0.f);
            set(*h->processor,"monoBend",polyphonic?1.f:0.f);
        }
        // This source ends before the helper's optional vibrato starts. The
        // comparison isolates ineffective host automation from pitch/timbre
        // changes and includes the eventual real release on both paths.
        const auto wave=bentGuitar(48000,57,0,.55);
        baseline.run(12000,wave);automated.run(12000,wave);
        check(baseline.sounding(1,57)&&automated.sounding(1,57),"Ignored-automation fixture did not acquire its held note");
        for(int change=0;change<6;++change) {
            if(polyphonic)set(*automated.processor,"monoBend",change%2?1.f:0.f);
            set(*automated.processor,"bendRange",change%2?0.f:1.f);
            baseline.run(1920,wave,{}, {17,64,257,128});automated.run(1920,wave,{}, {17,64,257,128});
            check(automated.sounding(1,57),"Ineffective bend automation interrupted a held non-bend voice");
        }
        baseline.run(17280,wave);automated.run(17280,wave);
        check(baseline.notes(1,true).size()==1&&baseline.notes(1,false).size()==1,"Steady automation control fragmented without any automation");
        check(automated.events.size()==baseline.events.size(),"Ineffective bend automation added Note Off/retrigger/controller events");
        for(size_t i=0;i<baseline.events.size();++i) {
            const auto& expected=baseline.events[i];const auto& actual=automated.events[i];
            check(actual.sample==expected.sample&&actual.midi.getRawDataSize()==expected.midi.getRawDataSize()
                &&std::equal(expected.midi.getRawData(),expected.midi.getRawData()+expected.midi.getRawDataSize(),actual.midi.getRawData()),
                "Ineffective bend automation changed actual MIDI events or causal timestamps");
        }
        baseline.allReleased();automated.allReleased();
    }
}

void sealedGuitarCurves(const std::filesystem::path& fixtures) {
    for(const std::string name:{"bend_whole_step","bend_semitone","bend_vibrato","bend_fast_bend"}) {
        const auto original=readWave(fixtures/"pcm"/(name+".wav"));
        const auto truth=readCurve(fixtures/"pitch-curves.tsv",name);
        for(bool decayed:{false,true}) {
            if(decayed&&name!="bend_whole_step")continue;
            auto signal=original;
            const std::string label=name+(decayed?"_derived_decay48dB":"");
            if(decayed)for(size_t sample=0;sample<signal.size();++sample) {
                // A disclosed transformation of known PCM, not a newly sealed or
                // unseen recording: preserve phase/pitch and onset/damping while
                // reducing amplitude by48dB over the physical ringing interval.
                const double age=std::clamp((double(sample)-2400)/64800.,0.,1.);
                signal[sample]*=float(std::pow(10.,-48*age/20));
            }
            for(int range:{2,12}) {
                Harness h;set(*h.processor,"bendRange",range==12?1.f:0.f);
                h.run(int64_t(signal.size()),signal,{}, {17,64,257,128});
                const auto on=h.notes(2,true),off=h.notes(2,false);
                check(on.size()==1&&off.size()==1&&on[0].midi.getNoteNumber()==57&&off[0].midi.getNoteNumber()==57,
                    label+": MIDI adapter fragmented the physical bent string");
                check(on[0].sample>=2400&&on[0].sample<=4800,label+": useful MIDI onset exceeded 50ms observation budget");
                check(off[0].sample>=67008&&off[0].sample<=72000,label+": bent note ended early or failed to release");
                requireRangeBeforeOn(h,range);requireCentreAfterOff(h);h.allReleased();
                const auto wheelChanges=std::count_if(h.events.begin(),h.events.end(),[](const Raw& e){return e.midi.getChannel()==2&&e.midi.isPitchWheel();});
                check(wheelChanges>=20,label+": actual MIDI adapter lost the continuous expression updates");
                size_t event=0;int wheel=8192;std::vector<double> errors;double settledMaximum=0;int settled=0;
                for(size_t i=0;i<truth.size();++i) {
                    const auto& point=truth[i];
                    while(event<h.events.size()&&h.events[event].sample<=point.sample) {
                        const auto& message=h.events[event++].midi;
                        if(message.getChannel()==2&&message.isPitchWheel())wheel=message.getPitchWheelValue();
                    }
                    // Score the actually emitted/held wheel at this time. No
                    // interpolation using future events, lag compensation, or
                    // comparison only when the detector chose to emit a change.
                    if(point.sample<8160||point.sample>=67008)continue;
                    const double reconstructed=57+double(wheel-8192)*range/8192.;
                    const double error=std::abs(reconstructed-point.pitch);errors.push_back(error);
                    if(i>=24&&i+24<truth.size()) {
                        double minimum=point.pitch,maximum=point.pitch;
                        for(size_t j=i-24;j<=i+24;++j){minimum=std::min(minimum,truth[j].pitch);maximum=std::max(maximum,truth[j].pitch);}
                        if(maximum-minimum<=.01){settledMaximum=std::max(settledMaximum,error);++settled;}
                    }
                }
                check(errors.size()>=800&&(name=="bend_vibrato"||settled>=20),label+": insufficient output/truth coverage");
                std::sort(errors.begin(),errors.end());
                const double p95=errors[size_t(std::ceil(.95*errors.size()))-1],maximum=errors.back();
                std::cout<<"MEASURE known PCM "<<label<<", range"<<range<<": actual MIDI pitch P95="<<100*p95
                    <<"c, max="<<100*maximum<<"c, settled max="<<100*settledMaximum<<"c, n="<<errors.size()<<"\n";
                // Match the independently declared GuitarQuality bounds. The
                // wrapper's 14-bit quantisation does not justify relaxing them.
                check(p95<=.35&&maximum<=.75&&settledMaximum<=.20,label+": actual MIDI pitch exceeded sealed guitar bend bounds");
            }
        }
    }
}

void shutdownAndModeChanges() {
    for(int action=0;action<7;++action) {
        Harness h;const auto wave=bentGuitar(h.rate,57,1.0,1.2);
        const std::vector<Raw> external{{4800,juce::MidiMessage::noteOn(1,72,juce::uint8(88))}};
        h.run(26400,wave,external);
        check(h.sounding(2,57)&&h.sounding(1,72),"Lifecycle fixture did not acquire generated and external voices");
        const size_t before=h.events.size();
        if(action==0)set(*h.processor,"monoBend",0);
        if(action==1)set(*h.processor,"pitchEnabled",0);
        if(action==2)set(*h.processor,"pitchMode",1);
        if(action==3)set(*h.processor,"bendRange",1);
        if(action==4) {
            auto layout=h.processor->getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::disabled());
            check(h.processor->setBusesLayout(layout),"Could not disable sidechain during bend");
        }
        if(action==6)h.processor->requestPanic();
        h.block(action==6?0:128,wave,{},action==5);
        check(!h.sounding(2,57),"Changing bend/audio mode left the old bent voice held");
        requireCentreAfterOff(h);
        if(action!=6) {
            check(h.sounding(1,72),"Source-only bend shutdown released unrelated external note");
            for(size_t i=before;i<h.events.size();++i)
                check(!(h.events[i].midi.getChannel()==1&&h.events[i].midi.isNoteOff()),"Bend reconfiguration emitted external channel-1 Note Off");
        }
        set(*h.processor,"pitchEnabled",0);
        const std::vector<Raw> release{{h.position,juce::MidiMessage::noteOff(1,72)}};
        if(action!=6)h.block(128,{},release);else h.block(128,{});
        h.allReleased();
    }
}

void defaultsPolyAndState() {
    auto fresh=std::make_unique<FlysnapperProcessor>();
    check(get(*fresh,"monoBend")==0&&get(*fresh,"bendRange")==0,"New bend option changed legacy factory MIDI/channel behavior");
    Harness h;set(*h.processor,"monoBend",0);const auto wave=bentGuitar(h.rate,57,0,.45);
    h.run(36000,wave);
    check(h.notes(1,true).size()==1&&h.notes(2,true).empty(),"Bend-disabled mono changed its original output channel");
    for(const auto& e:h.events)check(!e.midi.isPitchWheel(),"Bend-disabled mono emitted generated pitchwheel");
    h.allReleased();
    Harness poly;set(*poly.processor,"pitchMode",1);poly.run(36000,wave);
    check(!poly.notes(1,true).empty()&&poly.notes(2,true).empty(),"Mono-bend option changed poly output channel");
    for(const auto& e:poly.events)check(!e.midi.isPitchWheel(),"Poly mode incorrectly applied shared-channel mono bend");
    poly.allReleased();
    set(*fresh,"monoBend",1);set(*fresh,"bendRange",1);
    juce::MemoryBlock saved;fresh->getStateInformation(saved);
    auto restored=std::make_unique<FlysnapperProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
    check(get(*restored,"monoBend")==1&&get(*restored,"bendRange")==1,"Bend expression configuration did not roundtrip");
    auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));
    check(xml!=nullptr,"Missing saved bend XML");auto* parameters=xml->getChildByName("FLYSNAPPER_PARAMETERS");
    check(parameters!=nullptr,"Missing saved parameter tree");
    for(auto* child=parameters->getFirstChildElement();child;) {
        auto* next=child->getNextElement();const auto id=child->getStringAttribute("id");
        if(id=="monoBend"||id=="bendRange")parameters->removeChildElement(child,true);
        child=next;
    }
    juce::MemoryBlock legacy;juce::AudioProcessor::copyXmlToBinary(*xml,legacy);
    restored->setStateInformation(legacy.getData(),int(legacy.getSize()));
    check(get(*restored,"monoBend")==0&&get(*restored,"bendRange")==0,"Legacy state retained warm-instance bend/channel mode");
}
}

int runTests(const std::vector<std::filesystem::path>& args) {
    try {
        check(args.size()==2,"FlysnapperBendProcessorTests <guitar_controls directory>");
        juce::ScopedJuceInitialiser_GUI gui;
        continuousBendAndExternalExpression();correctedAnchorKeepsExpression();detunedRepicksRestoreWheelBeforeEveryOn();
        sealedGuitarCurves(args[1]);shutdownAndModeChanges();ignoredBendAutomationKeepsHeldNotes();defaultsPolyAndState();
        std::cout<<"FlysnapperBendProcessor: PASS ("<<checks<<" checks; audio-derived expression, not physical/Live latency).\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FlysnapperBendProcessor: FAIL: "<<e.what()<<"\n";return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::filesystem::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return runTests(args);}
#else
int main(int argc,char** argv){std::vector<std::filesystem::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return runTests(args);}
#endif
