// SPDX-License-Identifier: AGPL-3.0-or-later
// Actual processor sidechain -> dynamics -> detector -> corrected MIDI.
// Synthetic bounded controls prepare a real-guitar evaluation build; they do
// not certify arbitrary noise separation or physical interface/DAW latency.
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
thread_local bool watchNew=false;thread_local unsigned newCalls=0;
void* allocate(std::size_t n){if(watchNew)++newCalls;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
}
void* operator new(std::size_t n){return allocate(n);}void* operator new[](std::size_t n){return allocate(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}

namespace {
constexpr double pi=3.14159265358979323846;
unsigned checks=0;juce::Array<juce::var> cases;std::ostringstream csv;int64_t eventRows=0;
void check(bool value,const std::string& text){++checks;if(!value)throw std::runtime_error(text);}
void set(FlysnapperProcessor& p,const char* id,float value){auto* v=p.parameters.getParameter(id);check(v!=nullptr,std::string("Missing dynamics parameter ")+id);v->setValueNotifyingHost(v->convertTo0to1(value));}
float get(FlysnapperProcessor& p,const char* id){auto* v=p.parameters.getRawParameterValue(id);check(v!=nullptr,std::string("Missing dynamics value ")+id);return v->load();}
using Signal=std::function<float(int64_t)>;
struct Raw {int64_t sample;juce::MidiMessage midi;};
struct Harness {
    std::unique_ptr<FlysnapperProcessor> processor=std::make_unique<FlysnapperProcessor>();
    std::string name;double rate;int blockSize;int64_t position=0;
    std::vector<Raw> events;std::array<std::array<int,128>,16> active{};
    int deadlineMisses=0;double maxCpuUs=0;
    Harness(std::string caseName,double sampleRate=48000,int block=64):name(std::move(caseName)),rate(sampleRate),blockSize(block){
        auto layout=processor->getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::stereo());
        check(processor->setBusesLayout(layout),"Dynamics control could not activate sidechain");
        set(*processor,"mode",2);set(*processor,"scale",0);set(*processor,"correction",1);
        set(*processor,"pitchEnabled",1);set(*processor,"pitchMode",0);set(*processor,"monoBend",0);set(*processor,"mpeOutput",0);
        processor->prepareToPlay(rate,blockSize);
    }
    void block(int count,const Signal& signal,const std::vector<Raw>& input={},bool allocationCheck=false){
        juce::AudioBuffer<float> audio(2,count);juce::MidiBuffer midi;midi.ensureSize(131072);
        for(int n=0;n<count;++n){const float value=signal?signal(position+n):0;audio.setSample(0,n,value);audio.setSample(1,n,value);}
        for(const auto& e:input)if(e.sample>=position&&e.sample<position+count)midi.addEvent(e.midi,int(e.sample-position));
        newCalls=0;watchNew=allocationCheck;const auto begin=std::chrono::steady_clock::now();
        processor->processBlock(audio,midi);
        const auto end=std::chrono::steady_clock::now();watchNew=false;
        if(allocationCheck)check(newCalls==0,"Dynamics/calibration audio callback allocated C++ objects");
        const double us=std::chrono::duration<double,std::micro>(end-begin).count();maxCpuUs=std::max(maxCpuUs,us);deadlineMisses+=us>1e6*count/rate;
        check(!processor->capacityFault(),"Dynamics control overflowed processor output capacity");
        for(int ch=0;ch<2;++ch)check(count==0||audio.getMagnitude(ch,0,count)==0,"Dynamics instrument leaked sidechain audio");
        for(const auto meta:midi){
            check(meta.samplePosition>=0&&meta.samplePosition<std::max(count,1),"Dynamics MIDI offset outside supplied causal block");
            const auto m=meta.getMessage();const int ch=m.getChannel();check(ch>=1&&ch<=16,"Invalid dynamics MIDI channel");
            if(m.isNoteOnOrOff()){
                check(ch!=16,"Dynamics path leaked chord control notes");auto& held=active[size_t(ch-1)][size_t(m.getNoteNumber())];
                if(m.isNoteOn()){check(held==0,"Dynamics path duplicated an active MIDI note");++held;}
                else {check(held>0,"Dynamics path emitted unmatched Note Off");--held;}
            }
            events.push_back({position+meta.samplePosition,m});const char type=m.isNoteOn()?'N':m.isNoteOff()?'F':m.isPitchWheel()?'P':'C';
            csv<<name<<','<<int(rate)<<','<<blockSize<<','<<position+meta.samplePosition<<','<<type<<','<<ch<<','<<(m.isNoteOnOrOff()?m.getNoteNumber():m.isController()?m.getControllerNumber():0)<<','<<(m.isNoteOn()?m.getVelocity():m.isController()?m.getControllerValue():m.isPitchWheel()?m.getPitchWheelValue():0)<<'\n';++eventRows;
        }
        position+=count;
    }
    void run(int64_t samples,const Signal& signal={},const std::vector<Raw>& input={},bool allocations=false){while(samples>0){const int n=int(std::min<int64_t>(samples,blockSize));block(n,signal,input,allocations);samples-=n;}}
    std::vector<Raw> notes(bool on,int note=-1)const{std::vector<Raw> result;for(const auto& e:events)if((on?e.midi.isNoteOn():e.midi.isNoteOff())&&(note<0||e.midi.getNoteNumber()==note))result.push_back(e);return result;}
    void released()const{for(const auto& ch:active)for(int held:ch)check(held==0,"Dynamics control left a note held");}
    juce::var report(const char* scope){auto* o=new juce::DynamicObject;o->setProperty("name",juce::String(name));o->setProperty("status","passed");o->setProperty("scope",scope);o->setProperty("rate",rate);o->setProperty("block",blockSize);o->setProperty("events",int(events.size()));o->setProperty("deadline_misses",deadlineMisses);o->setProperty("callback_max_us",maxCpuUs);o->setProperty("timing_environment","Shared cloud CPU; observed deadlines are not certified realtime bounds");return juce::var(o);}
};
struct Fixture {int note,gain;const char* file;const char* hash;};
constexpr std::array<Fixture,6> quietFixtures{{
    {31,1,"pcm/quiet_hum_31_gain1.wav","79dd00d38242e96290405ef27d762b86f785ba7b6aa27db21c1e489d59428e72"},
    {31,8,"pcm/quiet_hum_31_gain8.wav","fe89c188d7b9e027cb844e2fa550529852790e3217a9f528947f01b10353bc55"},
    {40,1,"pcm/quiet_hum_40_gain1.wav","8c62198f2bae6a37ac37e99473b73ee79fbb9d89b889e15a42c138342a3e0800"},
    {40,8,"pcm/quiet_hum_40_gain8.wav","5cfbd9ca0f073c54ffce72512d9963fd2139b1b48623d8346471919ca97227eb"},
    {52,1,"pcm/quiet_hum_52_gain1.wav","06abeaf4e157f5e6db0320f4cc163219b67fa1d8d2a8eb5c20f8f260d9c116e2"},
    {52,8,"pcm/quiet_hum_52_gain8.wav","81ad7463ad58838f24ef9db4d48a4f6ed75df9ee0de0290cf9d2eb1333b04320"}
}};
std::vector<float> readFixture(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);check(bool(file),"Cannot read sealed quiet-hum PCM");
    const std::vector<uint8_t> b((std::istreambuf_iterator<char>(file)),{});
    const auto u16=[&](size_t at){if(at+2>b.size())throw std::runtime_error("Truncated quiet-hum WAV");return uint16_t(b[at])|(uint16_t(b[at+1])<<8);};
    const auto u32=[&](size_t at){return uint32_t(u16(at))|(uint32_t(u16(at+2))<<16);};
    check(b.size()>=12&&std::string(reinterpret_cast<const char*>(b.data()),4)=="RIFF"&&std::string(reinterpret_cast<const char*>(b.data()+8),4)=="WAVE","Invalid quiet-hum WAV");
    int format=0,channels=0,rate=0,bits=0;size_t data=0,length=0;
    for(size_t at=12;at+8<=b.size();){const auto n=u32(at+4);check(at+8+uint64_t(n)<=b.size(),"Quiet-hum WAV chunk outside file");const std::string tag(reinterpret_cast<const char*>(b.data()+at),4);
        if(tag=="fmt "){check(n>=16,"Short quiet-hum WAV format");format=u16(at+8);channels=u16(at+10);rate=int(u32(at+12));bits=u16(at+22);}if(tag=="data"){data=at+8;length=n;}at+=8+n+(n&1);}
    check(format==1&&channels==1&&rate==48000&&bits==16&&data&&!(length&1),"Quiet-hum fixture must be48kHz monoPCM16");
    std::vector<float> audio(length/2);for(size_t n=0;n<audio.size();++n)audio[n]=float(int16_t(u16(data+2*n)))/32768.f;return audio;
}
void quietHumRegressions(const std::filesystem::path& fixtureRoot){
    for(const auto& fixture:quietFixtures)for(bool bend:{false,true}){
        const std::string name="sealed_quiet_hum_"+std::to_string(fixture.note)+"_gain"+std::to_string(fixture.gain)+(bend?"_mono_bend":"_mono_notes");Harness h(name);
        set(*h.processor,"monoBend",bend?1.f:0.f);const auto samples=std::make_shared<std::vector<float>>(readFixture(fixtureRoot/std::filesystem::u8path(fixture.file)));
        const Signal signal=[samples](int64_t n){return n>=0&&size_t(n)<samples->size()?(*samples)[size_t(n)]:0.f;};h.run(int64_t(samples->size())+12000,signal);
        const auto ons=h.notes(true),offs=h.notes(false);check(ons.size()==1&&ons[0].midi.getNoteNumber()==fixture.note,"Default frontend fragmented, missed or added a quiet note beside mains");
        check(offs.size()==1&&offs[0].midi.getNoteNumber()==fixture.note,"Quiet mains control did not balance its one note");
        check(ons[0].sample>=24000&&ons[0].sample<74400,"Quiet mains decision was backdated or after physical damping");
        check(offs[0].sample>=73920&&offs[0].sample<=78240,"Quiet mains note ended early or more than80ms after physical damping");h.released();
        auto report=h.report("Exact sealed Nov4 controlled guitar-like/mains PCM; default frontend, separate mono-notes and mono-bend paths; no fixture/label changes");auto* o=report.getDynamicObject();
        o->setProperty("fixture_file",fixture.file);o->setProperty("expected_fixture_sha256",fixture.hash);o->setProperty("pitch",fixture.note);o->setProperty("mono_bend",bend);o->setProperty("reference_on_sample",24000);o->setProperty("decision_sample",juce::int64(ons[0].sample));o->setProperty("latency_ms",1000*(ons[0].sample-24000)/h.rate);o->setProperty("reference_off_sample",74400);o->setProperty("actual_off_sample",juce::int64(offs[0].sample));cases.add(report);
    }
}
float stringSample(double time,int note,double amplitude,int harmonics=8){
    const double phase=2*pi*440*std::pow(2.,(note-69)/12.)*time;double sum=0;
    for(int h=1;h<=harmonics;++h)sum+=std::sin(h*phase+.17*h)/std::pow(double(h),1.25);
    return float(amplitude*sum);
}
Signal pluck(double rate,int note,double amplitude=.045,double start=.08,double stop=.70){
    return [=](int64_t n){const double t=n/rate;if(t<start||t>=stop)return 0.f;const double local=t-start;return stringSample(local,note,amplitude*std::min(1.,local/.001)*std::exp(-local*.35));};
}
void defaultGateAndTail(){
    {
        Harness h("default_gate_below_threshold_tonal_interference");
        h.run(48000,[](int64_t n){return stringSample(n/48000.,57,.00014);});
        check(h.notes(true).empty(),"Default opening gate admitted an intentionally sub-threshold tonal interference");h.released();
        cases.add(h.report("Default dynamics; unwanted low-level periodic signal lies below the declared opening threshold, not a universal noise classifier"));
    }
    {
        Harness h("manual_hysteresis_held_tail_and_damping");set(*h.processor,"gateThresholdDb",-50);set(*h.processor,"gateHysteresisDb",12);
        const Signal wave=[](int64_t n){const double t=n/48000.;if(t<.08||t>=1.05)return 0.f;const double local=t-.08;const double a=t<.35?.04:.0016;return stringSample(local,57,a*std::min(1.,local/.001),3);};
        h.run(64800,wave);const auto ons=h.notes(true),offs=h.notes(false);check(ons.size()==1&&ons[0].midi.getNoteNumber()==57,"Held-tail control acquired an extra/wrong note or missed A3");
        check(offs.size()==1&&offs[0].midi.getNoteNumber()==57,"Held-tail control lost balanced release");
        check(offs[0].sample>=49920,"Hysteresis tail ended more than10ms before physical damping");
        check(offs[0].sample<=55200,"Physically damped tail continued beyond100ms");h.released();
        auto report=h.report("Manual threshold-50/hysteresis12; soft held tail falls below opening threshold but remains above closing threshold, then source is truly damped");
        report.getDynamicObject()->setProperty("reference_off_sample",50400);report.getDynamicObject()->setProperty("actual_off_sample",juce::int64(offs[0].sample));cases.add(report);
    }
}
void latencyByPitch(){
    for(double rate:{44100.,48000.,96000.})for(int block:{32,64,128})for(int note:{40,45,52,57,64,69,76}){
        const std::string name="default_note_"+std::to_string(note)+"_"+std::to_string(int(rate))+"_"+std::to_string(block);Harness h(name,rate,block);
        const int64_t onset=std::llround(.08*rate),damped=std::llround(.70*rate);h.run(int64_t(rate),pluck(rate,note));
        const auto ons=h.notes(true),offs=h.notes(false);check(ons.size()==1&&ons[0].midi.getNoteNumber()==note,"Default dynamics latency control missed or added a note");
        check(offs.size()==1&&offs[0].midi.getNoteNumber()==note,"Default dynamics note lacked balanced Note Off");
        check(ons[0].sample>=onset&&ons[0].sample<damped,"First correct decision was backdated or after source damping");
        check(offs[0].sample>=damped-std::llround(.01*rate),"Default dynamics caused premature release on a sustained clean source");h.released();
        auto report=h.report("Synthetic eight-partial unbent source through default dynamics, detector and correction; latency is descriptive and has no absolute acceptance cutoff");
        auto* o=report.getDynamicObject();o->setProperty("pitch",note);o->setProperty("reference_on_sample",juce::int64(onset));o->setProperty("decision_sample",juce::int64(ons[0].sample));o->setProperty("latency_ms",1000*(ons[0].sample-onset)/rate);o->setProperty("reference_off_sample",juce::int64(damped));o->setProperty("actual_off_sample",juce::int64(offs[0].sample));cases.add(report);
    }
}
void genuineOctavesRemainDistinct(){
    // Same predeclared physical counters as the independent held-octave audit:
    // phase-continuous frequency steps, no new amplitude attack at the step.
    for(int lower:{31,40})for(bool upward:{true,false}){
        const int first=upward?lower:lower+12,last=upward?lower+12:lower;
        const std::string name="actual_true_octave_"+std::string(upward?"up_":"down_")+std::to_string(lower);Harness h(name);
        constexpr int onset=9600,step=31200,damping=52800,sourceFrames=damping+192;
        auto wave=std::make_shared<std::vector<float>>(sourceFrames+12000,0.f);double phase=0;
        for(int n=onset;n<sourceFrames;++n){const int pitch=n<step?first:last;phase+=2*pi*440*std::pow(2.,(pitch-69.)/12.)/48000;double value=0;
            for(int harmonic=1;harmonic<=12;++harmonic)value+=std::sin(harmonic*phase+.17*harmonic)/std::pow(double(harmonic),1.25);
            const double age=double(n-onset)/48000,attack=std::min(1.,age/.002),decay=n<damping?1.:std::max(0.,1.-double(n-damping)/192);
            (*wave)[size_t(n)]=float(.15*attack*std::exp(-.18*age)*decay*value);
        }
        const Signal signal=[wave](int64_t n){return n>=0&&size_t(n)<wave->size()?(*wave)[size_t(n)]:0.f;};h.run(int64_t(wave->size()),signal);
        const auto ons=h.notes(true),offs=h.notes(false);check(ons.size()==2&&offs.size()==2,"True octave change was lost, duplicated or held as the former note");
        check(ons[0].midi.getNoteNumber()==first&&ons[1].midi.getNoteNumber()==last,"True octave transition selected a wrong anchor");
        check(ons[0].sample>=onset&&ons[0].sample<step&&ons[1].sample>=step&&ons[1].sample<damping,"True octave decision was backdated or missed its source interval");
        const auto oldOff=h.notes(false,first),newOff=h.notes(false,last);check(oldOff.size()==1&&newOff.size()==1&&oldOff[0].sample>=step-480&&oldOff[0].sample<damping,"True octave change left its old note stuck or ended it early");
        check(newOff[0].sample>=damping-480&&newOff[0].sample<=damping+3840,"True octave source did not respect physical damping");h.released();
        auto report=h.report("Actual default-dynamics processor; genuine octave up/down with continuous oscillator phase, no attack reset,4ms physical damping; guards against blindly latching the old pitch");auto* o=report.getDynamicObject();
        o->setProperty("old_anchor",first);o->setProperty("new_anchor",last);o->setProperty("reference_step_sample",step);o->setProperty("new_decision_sample",juce::int64(ons[1].sample));o->setProperty("transition_latency_ms",(ons[1].sample-step)/48.);o->setProperty("old_actual_off_sample",juce::int64(oldOff[0].sample));o->setProperty("reference_off_sample",damping);o->setProperty("new_actual_off_sample",juce::int64(newOff[0].sample));cases.add(report);
    }
}
void fastFrettedSlidesRemainDiscrete(){
    // Phase-continuous fretting plateaus, not per-fret amplitude attacks.
    // Each 35ms interior note must survive the full sidechain/MIDI path.
    for(bool upward:{true,false}){
        Harness h(upward?"plain_fast_frets_up_35ms":"plain_fast_frets_down_35ms");
        constexpr int onset=12000,stride=1680,damping=37440;
        auto samples=std::make_shared<std::vector<float>>(48000,0.f);double phase=0;
        for(int n=onset;n<damping;++n){
            const int step=std::min(7,(n-onset)/stride),note=upward?57+step:64-step;
            phase+=2*pi*440*std::exp2((note-69)/12.)/48000;double value=0;
            for(int harmonic=1;harmonic<=12;++harmonic)value+=std::sin(harmonic*phase+.17*harmonic)/std::pow(double(harmonic),1.25);
            (*samples)[size_t(n)]=float(.08*std::min(1.,double(n-onset)/96)*value);
        }
        const Signal wave=[samples](int64_t n){return n>=0&&size_t(n)<samples->size()?(*samples)[size_t(n)]:0.f;};
        h.run(int64_t(samples->size()),wave);h.released();
        auto report=h.report("Plain mono, eight35ms semitone plateaus with continuous phase and no renewed attack; synthetic fretting control through the actual default-dynamics processor");
        juce::Array<juce::var> rows;const auto ons=h.notes(true),offs=h.notes(false);
        bool correct=ons.size()==8&&offs.size()==8;
        for(int step=0;step<8;++step){
            const int note=upward?57+step:64-step;
            const int start=onset+step*stride,stop=step<7?start+stride:damping;
            const auto found=h.notes(true,note);correct&=found.size()==1;
            auto* row=new juce::DynamicObject;row->setProperty("pitch",note);
            row->setProperty("reference_on_sample",start);row->setProperty("reference_off_sample",stop);
            row->setProperty("note_on_count",int(found.size()));
            if(!found.empty()){
                correct&=found.front().sample>=start&&found.front().sample<stop;
                row->setProperty("decision_sample",juce::int64(found.front().sample));
                row->setProperty("latency_ms",(found.front().sample-start)/48.);
            }
            rows.add(juce::var(row));
        }
        report.getDynamicObject()->setProperty("references",rows);
        if(!correct)report.getDynamicObject()->setProperty("status","failed");
        cases.add(report);
        check(correct,"Plain35ms fretted slide omitted a plateau, triggered a wrong note or decided after its note interval");
    }
}
void sixOpenStringsRemainSixSources(){
    // Reproduction of the four failures reported after the 0.3.0 guitar trial:
    // a clean 7ms-per-string strum must not omit strings, invent a lower
    // octave or repick a smoothly decaying source. No noise is injected.
    Harness h("six_open_strings_one_strum_no_false_sources");
    set(*h.processor,"pitchMode",1);
    constexpr std::array<int,6> strings{{40,45,50,55,59,64}};
    const Signal wave=[strings](int64_t n){
        const double t=double(n)/48000.;double value=0;
        for(size_t j=0;j<strings.size();++j){
            const double local=t-(.08+j*.007);if(local<0||local>.6)continue;
            const double frequency=440*std::exp2((strings[j]-69)/12.);
            const double amplitude=std::min(local/.002,1.)*std::exp(-local/1.1);
            for(int harmonic=1;harmonic<=8;++harmonic)
                value+=.08/strings.size()*amplitude/std::pow(double(harmonic),1.2)
                    *std::sin(2*pi*frequency*harmonic*local+harmonic*.391+j*.76);
        }
        return float(value);
    };
    h.run(48000,wave);h.released();
    auto report=h.report("Clean synthetic six-open-string strum,7ms string spacing,2ms attacks and smooth independent decays; no repeated excitation or noise; per-string decision times are descriptive, not physical-interface latency");
    auto* object=report.getDynamicObject();juce::Array<juce::var> rows;
    int missing=0,repeats=0,wrong=0;bool lifetime=true;
    for(const auto& event:h.notes(true))if(std::find(strings.begin(),strings.end(),event.midi.getNoteNumber())==strings.end())++wrong;
    for(size_t j=0;j<strings.size();++j){
        const auto ons=h.notes(true,strings[j]),offs=h.notes(false,strings[j]);
        missing+=ons.empty();repeats+=std::max(0,int(ons.size())-1);
        const int64_t onset=std::llround((.08+j*.007)*48000),cut=onset+28800;
        lifetime&=ons.size()==1&&offs.size()==1&&ons.front().sample>=onset
            &&ons.front().sample<cut&&offs.front().sample>=cut-480;
        auto* row=new juce::DynamicObject;row->setProperty("pitch",strings[j]);
        row->setProperty("reference_on_sample",juce::int64(onset));row->setProperty("reference_off_sample",juce::int64(cut));
        row->setProperty("note_on_count",int(ons.size()));row->setProperty("note_off_count",int(offs.size()));
        if(!ons.empty()){row->setProperty("decision_sample",juce::int64(ons.front().sample));row->setProperty("latency_ms",(ons.front().sample-onset)/48.);}
        rows.add(juce::var(row));
    }
    object->setProperty("references",rows);object->setProperty("missing_notes",missing);
    object->setProperty("repeated_note_ons",repeats);object->setProperty("wrong_note_ons",wrong);
    if(missing||repeats||wrong||!lifetime)object->setProperty("status","failed");
    cases.add(report);
    check(missing==0&&repeats==0&&wrong==0&&lifetime,
        "Clean six-string strum omitted sources, double-triggered, invented an unplayed note or released before damping");
}
Signal calibrationWave(double rate){return [=](int64_t n){const double t=n/rate;const float noise=float(.00012*std::sin(2*pi*60*t)+.00006*std::sin(2*pi*1379*t));if(t<3)return noise;
    const double local=t-3,part=std::fmod(local,1.2);if(part>.95)return noise;const double a=std::fmod(local,3.6)<1.2?.018:.075;
    return noise+stringSample(part,57,a*std::min(1.,part/.003)*std::exp(-part*.9),8);};}
void settleTimers(){juce::Thread::sleep(65);juce::Timer::callPendingTimersSynchronously();}
void calibrationAndOverrides(){
    using Phase=FlysnapperProcessor::InputCalibrationSnapshot::Phase;
    {
        Harness h("calibration_start_releases_generated_preserves_external");
        const Signal wave=pluck(h.rate,57,.045,0,.8);const std::vector<Raw> input{{128,juce::MidiMessage::noteOn(1,84,juce::uint8(93))}};
        h.run(9600,wave,input);check(h.active[0][57]==1&&h.active[0][84]==1,"Calibration lifecycle setup did not acquire generated and external notes");
        const auto requestAt=h.position;h.processor->requestInputCalibration();h.block(64,wave);
        check(h.active[0][57]==0&&h.active[0][84]==1,"Starting calibration cut external MIDI or failed to release its generated note");
        const auto offs=h.notes(false,57);check(offs.size()==1&&offs[0].sample==requestAt,"Calibration did not release generated MIDI at its actual first callback");
        h.processor->cancelInputCalibration();h.block(64,{},{{h.position+32,juce::MidiMessage::noteOff(1,84)}});h.released();cases.add(h.report("Starting learning stops only generated audio notes; an existing external note retains its original binding and release"));
    }
    {
        Harness h("calibration_complete_visible_parameters_external_chord");set(*h.processor,"mode",0);const auto oldGate=get(*h.processor,"gateThresholdDb");const auto oldRatio=get(*h.processor,"compressorRatio");
        const std::vector<Raw> external{{0,juce::MidiMessage::noteOn(16,48,juce::uint8(90))},{0,juce::MidiMessage::noteOn(16,52,juce::uint8(90))},{0,juce::MidiMessage::noteOn(16,55,juce::uint8(90))},{640,juce::MidiMessage::noteOn(1,61,juce::uint8(90))},{96000,juce::MidiMessage::noteOff(1,61)}};
        h.processor->requestInputCalibration();h.block(64,calibrationWave(h.rate),external,true);check(h.processor->getInputCalibrationSnapshot().phase==Phase::Noise,"Calibration did not enter the quiet phase");
        h.run(153536,calibrationWave(h.rate),external,true);check(h.processor->getInputCalibrationSnapshot().phase==Phase::Playing,"Calibration did not progress from3s quiet to playing");
        check(get(*h.processor,"gateThresholdDb")==oldGate&&get(*h.processor,"compressorRatio")==oldRatio,"Calibration changed real parameters before completion");
        h.run(806400,calibrationWave(h.rate),external,true);auto state=h.processor->getInputCalibrationSnapshot();
        check(state.phase==Phase::Complete&&state.pending&&!state.applied,"Successful20s sample was not pending a message-thread parameter application");
        const auto ons=h.notes(true),offs=h.notes(false);check(ons.size()==1&&offs.size()==1&&ons[0].midi.getNoteNumber()==60&&ons[0].sample==640&&offs[0].sample==96000,"Calibration muted external MIDI, failed chord correction or leaked generated playing notes");
        check(h.processor->getSnapshot().chordMask==145,"Calibration discarded held channel16 chord tones");
        settleTimers();state=h.processor->getInputCalibrationSnapshot();check(state.applied&&!state.pending,"Message-thread calibration did not apply actual host parameters");
        const float learned=get(*h.processor,"gateThresholdDb");check(std::abs(learned-oldGate)>.1f&&std::isfinite(learned),"Successful calibration did not move the visible gate knob");
        // Exercise the learned settings, not only the UI parameter movement.
        // D4 is outside the still-held C chord, so correction must bind E4.
        const int64_t learnedOn=h.position+3840,learnedOff=h.position+31200;
        h.run(48000,pluck(h.rate,62,.004,double(learnedOn)/h.rate,double(learnedOff)/h.rate));
        const auto learnedOns=h.notes(true,64),learnedOffs=h.notes(false,64);
        check(h.notes(true).size()==2&&learnedOns.size()==1&&learnedOffs.size()==1,"Learned settings missed a soft new note, leaked an old calibration note, or failed chord correction");
        check(learnedOns[0].sample>=learnedOn&&learnedOns[0].sample<learnedOff,"Learned-note decision was backdated or missed the actual source");
        check(learnedOffs[0].sample>=learnedOff-480&&learnedOffs[0].sample<=learnedOff+3840,"Learned settings caused early release or delayed damping on the new soft note");
        set(*h.processor,"gateThresholdDb",learned+2);settleTimers();check(std::abs(get(*h.processor,"gateThresholdDb")-(learned+2))<.11f,"Manual override was overwritten by completed calibration");
        auto report=h.report("20-second synthetic calibration:3s stationary quiet noise then17s soft/strong periodic playing; no fit to the evaluated noise fixtures");
        auto* o=report.getDynamicObject();o->setProperty("noise_floor_db",state.noiseFloorDb);o->setProperty("soft_level_db",state.softLevelDb);o->setProperty("strong_level_db",state.strongLevelDb);o->setProperty("learned_gate_db",learned);o->setProperty("manual_gate_db",get(*h.processor,"gateThresholdDb"));o->setProperty("learned_source_pitch",62);o->setProperty("learned_corrected_note",64);o->setProperty("learned_reference_on_sample",juce::int64(learnedOn));o->setProperty("learned_decision_sample",juce::int64(learnedOns[0].sample));o->setProperty("learned_reference_off_sample",juce::int64(learnedOff));o->setProperty("learned_actual_off_sample",juce::int64(learnedOffs[0].sample));cases.add(report);h.released();
    }
    {
        Harness h("calibration_silence_failure_preserves_parameters");const float before=get(*h.processor,"gateThresholdDb");h.processor->requestInputCalibration();h.run(960000,{}, {},true);settleTimers();const auto state=h.processor->getInputCalibrationSnapshot();
        check(state.phase==Phase::Failed&&!state.applied&&get(*h.processor,"gateThresholdDb")==before,"No-playing calibration changed settings or succeeded");check(h.notes(true).empty(),"Failed silent calibration generated notes");h.released();cases.add(h.report("20secondsdigital silence; insufficient playing must fail without changing settings"));
    }
    {
        Harness h("calibration_cancel_restart_manual_change");const float before=get(*h.processor,"gateThresholdDb");h.processor->requestInputCalibration();h.run(24000,calibrationWave(h.rate));h.processor->cancelInputCalibration();h.run(64,calibrationWave(h.rate));settleTimers();
        check(h.processor->getInputCalibrationSnapshot().phase==Phase::Cancelled&&get(*h.processor,"gateThresholdDb")==before,"Cancelled sample changed parameters");
        h.processor->requestInputCalibration();h.run(128,calibrationWave(h.rate));set(*h.processor,"gateThresholdDb",-43);h.run(64,calibrationWave(h.rate));settleTimers();
        check(h.processor->getInputCalibrationSnapshot().phase==Phase::Failed&&get(*h.processor,"gateThresholdDb")==-43,"Manual change during learning was overwritten");
        h.processor->requestInputCalibration();h.run(128,calibrationWave(h.rate));set(*h.processor,"inputGainDb",2);h.run(64,calibrationWave(h.rate));settleTimers();
        check(h.processor->getInputCalibrationSnapshot().phase==Phase::Failed&&get(*h.processor,"inputGainDb")==2,"Input gain change during learning was overwritten or failed to invalidate the sample");
        h.processor->requestInputCalibration();h.run(128,calibrationWave(h.rate));set(*h.processor,"autoInput",0);h.run(64,calibrationWave(h.rate));settleTimers();
        check(h.processor->getInputCalibrationSnapshot().phase==Phase::Failed&&h.processor->getInputCalibrationSnapshot().failureReason==6&&get(*h.processor,"gateThresholdDb")==-43,"Changing adaptive tracking during learning kept an invalid noise history or changed knobs");
        h.processor->requestInputCalibration();h.run(128,calibrationWave(h.rate));h.processor->reset();h.run(64,calibrationWave(h.rate));settleTimers();
        check(h.processor->getInputCalibrationSnapshot().phase==Phase::Failed&&h.processor->getInputCalibrationSnapshot().failureReason==6&&get(*h.processor,"gateThresholdDb")==-43,"Host reset during learning kept a partial model or overwrote parameters");
        h.processor->requestInputCalibration();h.run(64,calibrationWave(h.rate));h.processor->prepareToPlay(96000,64);check(h.processor->getInputCalibrationSnapshot().phase==Phase::Idle&&!h.processor->getInputCalibrationSnapshot().pending,"Sample-rate preparation kept a partial calibration active");
        h.released();cases.add(h.report("Cancel/restart/manual gate or gain change, tracking-mode change, host reset and sample-rate preparation invalidate partial learning"));
    }
    {
        Harness h("calibration_sidechain_loss_preserves_parameters");const float before=get(*h.processor,"gateThresholdDb");h.processor->requestInputCalibration();h.run(128,calibrationWave(h.rate));
        auto layout=h.processor->getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::disabled());check(h.processor->setBusesLayout(layout),"Could not simulate sidechain loss during learning");h.run(64,{});settleTimers();
        const auto state=h.processor->getInputCalibrationSnapshot();check(state.phase==Phase::Failed&&state.failureReason==5&&!state.applied&&get(*h.processor,"gateThresholdDb")==before,"Sidechain loss applied a partial calibration or failed to report missing input");h.released();cases.add(h.report("A disconnected sidechain invalidates partial learning without changing existing parameters"));
    }
}
void writeEvidence(const char* status,const juce::String& error={}){
    auto* object=new juce::DynamicObject;juce::var report(object);object->setProperty("schema",1);object->setProperty("kind","FlysnapperDynamicsProcessor");object->setProperty("status",status);object->setProperty("checks",int(checks));object->setProperty("cases",cases);object->setProperty("events_file","Flysnapper-dynamics-events.csv");object->setProperty("events_rows",juce::int64(eventRows));object->setProperty("physical_latency_measured",false);object->setProperty("polyphonic_bending_required",false);object->setProperty("absolute_latency_acceptance_limit_ms",juce::var());if(error.isNotEmpty())object->setProperty("error",error);
    auto* fixtures=new juce::DynamicObject;auto* files=new juce::DynamicObject;for(const auto& item:quietFixtures)files->setProperty(item.file,item.hash);
    fixtures->setProperty("fixture_root","Tests/AudioFixtures/readiness");fixtures->setProperty("seal_sha256","d5af9fb3fefb4d41918e62810d59290c1dd741d95337d58ec34efcdcf14f9fba");fixtures->setProperty("hash_role","Expected input identity pinned in test source; builder independently verifies actual bytes before/after tests and packaging");fixtures->setProperty("files_sha256",juce::var(files));object->setProperty("fixture_inputs",juce::var(fixtures));
    const auto cwd=juce::File::getCurrentWorkingDirectory();if(!cwd.getChildFile("Flysnapper-dynamics-events.csv").replaceWithText(juce::String::fromUTF8(csv.str().c_str())))throw std::runtime_error("Cannot write actual dynamics MIDI evidence");
    if(!cwd.getChildFile("Flysnapper-dynamics-evaluation.json").replaceWithText(juce::JSON::toString(report,true)+"\n"))throw std::runtime_error("Cannot write actual dynamics test report");
}
}
int run(const std::vector<std::filesystem::path>& args){
    csv<<"case,rate,block,sample,type,channel,note,value\n";csv<<std::setprecision(9);
    try{check(args.size()==2,"FlysnapperDynamicsProcessorTests <AudioFixtures root>");juce::ScopedJuceInitialiser_GUI runtime;defaultGateAndTail();latencyByPitch();genuineOctavesRemainDistinct();calibrationAndOverrides();quietHumRegressions(args[1]/"readiness");fastFrettedSlidesRemainDiscrete();sixOpenStringsRemainSixSources();writeEvidence("passed");std::cout<<"FlysnapperDynamicsProcessor: PASS ("<<checks<<" checks, "<<cases.size()<<" bounded cases; not physical latency or universal guitar acceptance).\n";return 0;}
    catch(const std::exception& error){watchNew=false;try{writeEvidence("failed",error.what());}catch(...){}std::cerr<<"FlysnapperDynamicsProcessor: FAIL: "<<error.what()<<'\n';return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::filesystem::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return run(args);}
#else
int main(int argc,char** argv){std::vector<std::filesystem::path> args;for(int i=0;i<argc;++i)args.emplace_back(std::filesystem::u8path(argv[i]));return run(args);}
#endif
