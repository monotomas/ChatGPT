// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <atomic>
#include <new>
#include <iostream>
#include <limits>
#include <memory>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <stdexcept>
#include <vector>
namespace {std::atomic<bool> countAllocations{false};std::atomic<size_t> allocationCount{0};}
void* operator new(std::size_t n){if(countAllocations.load(std::memory_order_relaxed))allocationCount.fetch_add(1,std::memory_order_relaxed);if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
using namespace flysnapper::pitch;
namespace {
int checks=0;
void check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
struct Seen {bool on;int note,velocity,sample;};
std::vector<float> tone(int rate,int note,int length,bool harmonics=true){
    std::vector<float> out(size_t(length),0);double f=440*std::pow(2.0,(note-69)/12.0);
    for(int i=0;i<length;++i){double x=0,t=double(i)/rate;for(int h=1;h<=(harmonics?12:1);++h)if(f*h<rate*.45)x+=std::sin(6.283185307179586*f*h*t+.17*h)/std::pow(double(h),1.25);out[size_t(i)]=float(.15*x*std::min(1.0,t/.001));}
    return out;
}
std::vector<Seen> run(PitchDetector& detector,const std::vector<float>& audio,int block){
    std::vector<Seen> out;Output events;
    for(int i=0;i<int(audio.size());i+=block){events.clear();int count=std::min(block,int(audio.size())-i);detector.process(audio.data()+i,count,events);check(!events.overflow,"bounded normal output overflowed");
        for(size_t j=0;j<events.size;++j){const auto& e=events.events[j];check(e.offset>=0&&e.offset<count,"sample offset outside supplied block");check(e.note>=28&&e.note<=100,"detected pitch outside range");check(e.velocity>=0&&e.velocity<=127,"invalid velocity");out.push_back({e.on,e.note,e.velocity,i+e.offset});}}
    return out;
}
void silenceAndInvalid(){
    auto d=std::make_unique<PitchDetector>();d->prepare(48000);std::vector<float> silence(4800,0);
    auto result=run(*d,silence,32);check(result.empty(),"silence generated a note");
    silence[12]=std::numeric_limits<float>::quiet_NaN();silence[50]=std::numeric_limits<float>::infinity();
    result=run(*d,silence,127);check(result.empty(),"nonfinite input generated notes");
    check(std::isfinite(d->status().levelDb),"nonfinite status");
}
void monoAndBlocks(){
    for(int rate:{44100,48000,96000})for(int block:{32,127,512})for(int note:{40,52,60,69,84}){
        auto d=std::make_unique<PitchDetector>();d->prepare(rate);auto a=tone(rate,note,rate/3);a.resize(size_t(rate/2),0);
        auto out=run(*d,a,block);int ons=0,offs=0;
        for(auto e:out){check(e.note==note,"monophonic stable harmonic tone emitted wrong note");if(e.on){++ons;check(e.sample<rate*.16,"monophonic acquisition functional bound exceeded");check(e.velocity>0,"note on lacks velocity");}else ++offs;}
        check(ons==1&&offs==1,"monophonic note lifecycle mismatch");check(d->status().activeNotes==0,"monophonic voice left active");
    }
}
void polyphony(){
    for(const auto& notes:{std::vector<int>{60,64,67},std::vector<int>{48,52,55},std::vector<int>{52,53}}){
        auto d=std::make_unique<PitchDetector>();d->prepare(48000);Settings settings;settings.polyphonic=true;d->configure(settings);
        std::vector<float> a(24000,0);for(int note:notes){auto n=tone(48000,note,16000);for(size_t i=0;i<n.size();++i)a[i]+=n[i]/float(notes.size());}
        auto out=run(*d,a,128);std::vector<int> actual;int offs=0;
        for(auto e:out)if(e.on)actual.push_back(e.note);else ++offs;
        std::sort(actual.begin(),actual.end());auto expected=notes;std::sort(expected.begin(),expected.end());
        check(actual==expected,"polyphonic chord emitted wrong or missing note");check(offs==int(notes.size()),"polyphonic note offs mismatch");check(d->status().activeNotes==0,"polyphonic voice left active");
    }
}
void declaredRangeAndRealtime(){
    auto detector=std::make_unique<PitchDetector>();detector->prepare(48000);
    Output events;
    for(int rate:{44100,48000,96000})for(int note=PitchDetector::lowNote;note<=PitchDetector::highNote;++note){
        detector->prepare(rate);events.clear();detector->reset(events);auto audio=tone(rate,note,int(rate*.30));audio.resize(size_t(rate*.45),0);
        auto result=run(*detector,audio,128);int ons=0,offs=0;
        for(auto e:result){if(e.note!=note)std::cerr<<"Declared range expected="<<note<<" got="<<e.note<<" on="<<e.on<<" sample="<<e.sample<<"\n";check(e.note==note,"declared MIDI range produced an octave or semitone error");if(e.on)++ons;else ++offs;}
        check(ons==1&&offs==1,"declared detector range has missing or repeated notes");
    }
    // Stable detuning is a valid continuous pitch, not a permanent dead band.
    for(int rate:{44100,48000,96000})for(int note:{28,40,52,64,76,100})for(float cents:{-40.0f,40.0f})for(bool bright:{false,true}){
        detector->prepare(rate);events.clear();detector->reset(events);std::vector<float> audio(size_t(rate/2),0);
        const double frequency=440*std::pow(2.0,(note-69+cents/100)/12.0);
        for(int i=0;i<int(rate/3);++i)for(int h=1;h<=(bright?12:1);++h)if(frequency*h<rate*.45)
            audio[size_t(i)]+=float((bright?.03:.15)*std::sin(6.283185307179586*frequency*h*i/rate+.17*h)/std::pow(double(h),.8));
        auto result=run(*detector,audio,64);int ons=0,offs=0;
        for(auto e:result){check(e.note==note,"stable detuned tone mapped to wrong semitone");if(e.on)++ons;else ++offs;}
        check(ons==1&&offs==1,"stable detuned tone had a dead zone or false bright-waveform retrigger");
    }
    detector->prepare(48000);auto audio=tone(48000,60,512);Settings settings;settings.polyphonic=true;detector->configure(settings);
    events.clear();detector->reset(events);detector->process(audio.data(),512,events);
    allocationCount.store(0);countAllocations.store(true);
    for(int i=0;i<64;++i){events.clear();detector->process(audio.data(),512,events);}
    countAllocations.store(false);
    check(allocationCount.load()==0,"prepared detector allocated C++ objects during audio processing");
    std::cout<<"PASS declared MIDI28..100 at44.1/48/96kHz, stable+/-40cent notes and prepared C++ allocation check (malloc/host wrapper excluded)\n";
}
void causalPrediction(){
    TimeDomainPursuit model;model.prepare(12000,512);
    std::array<float,544> wave{};
    for(int note:{48,52,55}){
        const double f=440*std::pow(2.0,(note-69)/12.0);
        for(int i=0;i<int(wave.size());++i)for(int h=1;h<=8;++h)
            wave[size_t(i)]+=float(.05*std::sin(6.283185307179586*f*h*(i+123)/12000+.11*h)/std::pow(double(h),1.25));
    }
    std::array<float,73> strength{},confidence{};std::array<float,32> prediction{};
    model.analyse(wave.data(),512,strength,confidence);
    check(model.copyPrediction(prediction),"valid harmonic frame lacked a causal next-hop prediction");
    double error=0,power=0;
    for(int i=0;i<32;++i){double actual=wave[size_t(512+i)],delta=actual-prediction[size_t(i)];error+=delta*delta;power+=actual*actual;}
    check(std::sqrt(error/std::max(power,1e-20))<.10,"harmonic prediction phase/amplitude mismatch on unseen next samples");
    model.analyse(nullptr,0,strength,confidence);
    check(!model.copyPrediction(prediction),"invalid frame retained a stale harmonic prediction");
}
void transitions(){
    auto d=std::make_unique<PitchDetector>();d->prepare(48000);auto a=tone(48000,60,8000);auto result=run(*d,a,64);check(std::any_of(result.begin(),result.end(),[](auto e){return e.on&&e.note==60;}),"transition setup note missing");
    Output out;d->reset(out,17);check(out.size==1&&!out.events[0].on&&out.events[0].note==60&&out.events[0].offset==17,"reset did not end held note");
    run(*d,a,64);Settings settings;settings.enabled=false;d->configure(settings);out.clear();d->process(a.data(),128,out);check(out.size==1&&!out.events[0].on,"disable did not end held note");
    out.clear();d->process(a.data(),128,out);check(out.size==0,"disabled detector still emits");settings.enabled=true;settings.polyphonic=true;d->configure(settings);run(*d,a,64);check(d->status().activeNotes<=6,"detector polyphony bound exceeded");
    out.clear();settings.polyphonic=false;d->configure(settings);d->process(a.data(),128,out);check(std::none_of(out.events.begin(),out.events.begin()+std::ptrdiff_t(out.size),[](auto e){return e.on;}),"mode change reused old audio onset");
}
}

std::vector<float> readRegressionWave(const std::filesystem::path& file){
    std::ifstream stream(file,std::ios::binary);
    if(!stream)throw std::runtime_error("Cannot open pitch lifecycle regression fixture");
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)),{});
    const auto u16=[&](size_t at){if(at+2>bytes.size())throw std::runtime_error("Truncated regression WAV");return uint16_t(bytes[at])|(uint16_t(bytes[at+1])<<8);};
    const auto u32=[&](size_t at){return uint32_t(u16(at))|(uint32_t(u16(at+2))<<16);};
    if(bytes.size()<12||std::string(reinterpret_cast<const char*>(bytes.data()),4)!="RIFF"
        ||std::string(reinterpret_cast<const char*>(bytes.data()+8),4)!="WAVE")throw std::runtime_error("Not a regression WAV");
    size_t data=0,length=0;int format=0,channels=0,rate=0,bits=0;
    for(size_t at=12;at+8<=bytes.size();){
        const uint32_t count=u32(at+4);
        if(at+8+uint64_t(count)>bytes.size())throw std::runtime_error("Regression WAV chunk outside file");
        const std::string name(reinterpret_cast<const char*>(bytes.data()+at),4);
        if(name=="fmt "){
            if(count<16)throw std::runtime_error("Truncated regression WAV format");
            format=u16(at+8);channels=u16(at+10);rate=int(u32(at+12));bits=u16(at+22);
        }else if(name=="data"){data=at+8;length=count;}
        at+=8+count+(count&1);
    }
    if(format!=1||channels!=1||rate!=48000||bits!=16||!data||(length&1))throw std::runtime_error("Regression fixture must be48kHz mono PCM16");
    std::vector<float> audio(length/2);
    for(size_t i=0;i<audio.size();++i)audio[i]=float(int16_t(u16(data+2*i)))/32768.f;
    return audio;
}
void acquiredOctaveContinuity(const std::filesystem::path& fixtures){
    auto detector=std::make_unique<PitchDetector>();detector->prepare(48000);
    Settings settings;settings.polyphonic=true;detector->configure(settings);
    const auto recorded=readRegressionWave(fixtures/"pcm"/"recorded_E_octaves.wav");
    const auto result=run(*detector,recorded,64);
    int ons=0,offs=0,first=-1,last=-1;
    for(auto event:result)if(event.note==52){if(event.on){++ons;first=event.sample;}else{++offs;last=event.sample;}}
    check(ons==1&&offs==1,"Acquired recorded middle octave was fragmented or missing");
    check(first>=0&&first<12000,"Recorded middle octave acquisition regression");
    check(last>=35520&&last<=40800,"Acquired middle octave ended before its source or failed to release within100ms");
    // A bright lower string must not keep an independently damped upper
    // string alive merely because its own high partials remain present.
    detector->prepare(48000);detector->configure(settings);
    const auto bright=readRegressionWave(fixtures/"regressions"/"bright_lower_upper_damped.wav");
    const auto damped=run(*detector,bright,64);
    int lowerOns=0,lowerOffs=0,upperOns=0,upperOffs=0,lowerEnd=-1,upperEnd=-1;
    for(auto event:damped){
        check(event.note==40||event.note==52,"Bright damping control emitted an unrelated pitch");
        if(event.note==40){if(event.on)++lowerOns;else{++lowerOffs;lowerEnd=event.sample;}}
        if(event.note==52){if(event.on)++upperOns;else{++upperOffs;upperEnd=event.sample;}}
    }
    check(lowerOns==1&&lowerOffs==1&&upperOns==1&&upperOffs==1,"Bright independent-damping lifecycle mismatch");
    check(lowerEnd>=30720&&lowerEnd<=36000,"Continuing bright lower voice ended early or hung");
    check(upperEnd>=13920&&upperEnd<=19200,"Damped upper voice followed the lower harmonics instead of releasing within100ms");
    std::cout<<"PASS acquired recorded octave continuity and independently damped upper under a bright lower string\n";
}
void repickPhaseEvidence(const std::filesystem::path& fixtures){
    auto detector=std::make_unique<PitchDetector>();Settings settings;settings.polyphonic=true;
    auto render=[&](const char* file){detector->prepare(48000);detector->configure(settings);return run(*detector,readRegressionWave(fixtures/"regressions"/file),64);};
    for(const auto& item:std::array<std::pair<const char*,int>,2>{{{"held_stiff_E2.wav",33600},{"twelfth_withdrawal_E2.wav",36000}}}){
        const auto events=render(item.first);int ons=0,offs=0,last=-1;
        for(auto event:events)if(event.note==40){if(event.on)++ons;else{++offs;last=event.sample;}}
        check(ons==1&&offs==1,"Steady lower note was falsely restarted by period bias or upper withdrawal");
        check(last>=item.second-480&&last<=item.second+4800,"Steady lower note released early or beyond100ms");
    }
    // Distinct plucks can replace a string without an intervening silent gap.
    // Preserve actual repicks even when its fundamental happens to change
    // phase very little: independently changing odd partials supply evidence.
    const auto repicks=render("continuous_repick_E2.wav");std::vector<int> starts;int offs=0;
    for(auto event:repicks){check(event.note==40,"Same-pitch articulation emitted an unrelated note");if(event.on)starts.push_back(event.sample);else ++offs;}
    check(starts.size()==3&&offs==3,"Phase filtering suppressed a genuine continuous-string repick");
    for(size_t i=0;i<starts.size();++i){const int onset=2400+int(i)*9600;check(starts[i]>=onset&&starts[i]<=onset+5760,"Repick exceeded120ms functional observation bound");}
    std::cout<<"PASS stiff held string, upper-withdrawal stability and three genuine continuous repicks\n";
}
void dynamicsAdmissionLifecycle(){
    auto detector=std::make_unique<PitchDetector>();detector->prepare(48000);
    Settings settings;settings.autoInput=false;settings.thresholdDb=-70;settings.allowNewNotes=false;detector->configure(settings);
    auto first=tone(48000,57,48000);auto next=tone(48000,64,28800);std::vector<float> silence(12000,0);
    auto seen=run(*detector,std::vector<float>(first.begin(),first.begin()+12000),64);
    check(std::none_of(seen.begin(),seen.end(),[](auto e){return e.on;}),"Closed dynamics admission allowed a new note");
    settings.allowNewNotes=true;detector->configure(settings);
    seen=run(*detector,std::vector<float>(first.begin()+12000,first.begin()+24000),64);
    check(std::count_if(seen.begin(),seen.end(),[](auto e){return e.on&&e.note==57;})==1,"Opening dynamics admission did not acquire the audible note");
    settings.allowNewNotes=false;detector->configure(settings);
    seen=run(*detector,std::vector<float>(first.begin()+24000,first.end()),64);
    check(seen.empty(),"Closing admission reset or rearticulated a held note");
    check(detector->status().activeNotes==1,"Closed admission suppressed an already held note");
    seen=run(*detector,silence,64);
    check(std::count_if(seen.begin(),seen.end(),[](auto e){return !e.on&&e.note==57;})==1&&detector->status().activeNotes==0,"Closed admission prevented release after actual source withdrawal");
    seen=run(*detector,std::vector<float>(next.begin(),next.begin()+14400),64);
    check(std::none_of(seen.begin(),seen.end(),[](auto e){return e.on;}),"Admission stayed closed but a later source was acquired");
    settings.allowNewNotes=true;detector->configure(settings);
    seen=run(*detector,std::vector<float>(next.begin()+14400,next.end()),64);
    check(std::count_if(seen.begin(),seen.end(),[](auto e){return e.on&&e.note==64;})==1,"Reopening admission did not acquire a fresh source");
    run(*detector,silence,64);check(detector->status().activeNotes==0,"Reopened source hung after silence");
    std::cout<<"PASS separate new-note admission, held-note continuation and actual withdrawal release\n";
}
void staticPolyVoiceIdentity(){
    auto detector=std::make_unique<PitchDetector>();detector->prepare(48000);
    Settings settings;settings.polyphonic=true;settings.polyBend=false;detector->configure(settings);
    auto audio=tone(48000,57,36000);const auto upper=tone(48000,64,36000);
    for(size_t i=0;i<audio.size();++i)audio[i]=.6f*(audio[i]+upper[i]);
    std::array<int,6> active;active.fill(-1);std::array<int,128> ids;ids.fill(-1);Output out;int ons=0;
    const auto observe=[&](const Output& events){for(size_t i=0;i<events.size;++i){const auto& e=events.events[i];
        check(!e.pitchChange,"Fixed-semitone poly mode emitted continuous pitch");
        check(e.voiceId>=0&&e.voiceId<6,"Static poly event lacks a bounded independent voice ID");
        if(e.on){check(active[size_t(e.voiceId)]<0,"Static poly reused an ID while another note was held");active[size_t(e.voiceId)]=e.note;ids[size_t(e.note)]=e.voiceId;++ons;}
        else {check(active[size_t(e.voiceId)]==e.note,"Static poly Note Off changed the source identity");active[size_t(e.voiceId)]=-1;}
    }};
    for(int at=0;at<int(audio.size());at+=64){out.clear();detector->process(audio.data()+at,std::min(64,int(audio.size())-at),out);observe(out);}
    check(ons>=2&&ids[57]>=0&&ids[64]>=0&&ids[57]!=ids[64],"Static two-source control did not acquire independent voices");
    check(active[size_t(ids[57])]==57&&active[size_t(ids[64])]==64,"Static voices released while both sources were still ringing");
    // configure() has already selected mono when the pending reset runs.
    // Old static-poly Off events still need their original identities.
    settings.polyphonic=false;detector->configure(settings);std::array<float,64> zero{};out.clear();detector->process(zero.data(),64,out);observe(out);
    check(std::all_of(active.begin(),active.end(),[](int note){return note<0;}),"Mode reset left a static-poly source held");
    std::cout<<"PASS fixed-semitone poly IDs, simultaneous voices and mode-change release identity\n";
}
void staticStepMissingRootGuard(){
    std::ofstream raw("Flysnapper-static-poly-step-events.csv");check(bool(raw),"Cannot retain static-poly step regression events");
    raw<<"partials,sample,type,note,voice_id\n";
    for(int partials:{3,4,5,6,8,12}){
        auto detector=std::make_unique<PitchDetector>();detector->prepare(48000);Settings settings;settings.polyphonic=true;settings.polyBend=false;detector->configure(settings);
        std::vector<float> audio(55200,0.f);double moving=0,held=0;
        for(int n=0;n<43200;++n){const double t=n/48000.;const int note=t<.4?59:61;moving+=2*3.14159265358979323846*440*std::pow(2.,(note-69.)/12.)/48000;held+=2*3.14159265358979323846*440*std::pow(2.,(64.-69.)/12.)/48000;
            double a=0,b=0;for(int h=1;h<=partials;++h){a+=std::sin(h*moving+.17*h)/std::pow(double(h),1.25);b+=std::sin(h*held+.11*h)/std::pow(double(h),1.25);}audio[size_t(n)]=float(std::min(1.,t/.002)*std::exp(-.18*t)*(.15*a+.11*b));}
        Output out;int on59=-1,on61=-1,on64=-1,off59=-1,off61=-1,off64=-1,otherOns=0;
        for(int at=0;at<int(audio.size());at+=64){out.clear();detector->process(audio.data()+at,std::min(64,int(audio.size())-at),out);for(size_t i=0;i<out.size;++i){const auto& e=out.events[i];check(!e.pitchChange,"Static step emitted continuous pitch");const int sample=at+e.offset;raw<<partials<<','<<sample<<','<<(e.on?'N':'F')<<','<<e.note<<','<<e.voiceId<<'\n';
                check(!e.on||e.note!=33,"Unsupported5th/7th-partial subharmonic33 stole genuine static sources");
                if(e.note==59){if(e.on)on59=sample;else off59=sample;}
                else if(e.note==61){if(e.on)on61=sample;else{check(sample>=42720,"Stepped note61 was cut before physical damping");off61=sample;}}
                else if(e.note==64){if(e.on)on64=sample;else{check(sample>=42720,"Held note64 was cut by an unsupported subharmonic");off64=sample;}}
                else if(e.on)++otherOns;
        }}
        check(on59>=0&&on59<19200&&on64>=0&&on64<19200,"Static step did not acquire both initial source notes");
        check(on61>=19200&&on61<43200,"Static semitone step did not acquire the replacement note");
        check(off59>=19200&&off59<=24000,"Former static note did not release within100ms of the source step");
        check(off61>=42720&&off61<=48000&&off64>=42720&&off64<=48000&&detector->status().activeNotes==0,"Static step failed balanced damping release");
        std::cout<<"MEASURE static59->61+held64 partials="<<partials<<" decision61="<<on61<<" latencyMs="<<(on61-19200)/48.<<" off61="<<off61<<" off64="<<off64<<" otherOnDiagnostics="<<otherOns<<" (unrelated overtone extras remain reported, not a zero-error claim)\n";
    }
}
int runTests(const std::vector<std::filesystem::path>& args){try{
    if(args.size()!=2)throw std::runtime_error("FlysnapperPitchTests <AudioFixtures directory>");
    silenceAndInvalid();monoAndBlocks();polyphony();causalPrediction();transitions();declaredRangeAndRealtime();acquiredOctaveContinuity(args[1]);repickPhaseEvidence(args[1]);dynamicsAdmissionLifecycle();staticPolyVoiceIdentity();staticStepMissingRootGuard();
    std::cout<<"FlysnapperPitch: PASS ("<<checks<<" checks; functional audio controls, not physical latency acceptance)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FlysnapperPitch: FAIL: "<<e.what()<<'\n';return 1;}}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::filesystem::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return runTests(args);}
#else
int main(int argc,char** argv){std::vector<std::filesystem::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return runTests(args);}
#endif
