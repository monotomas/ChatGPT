// SPDX-License-Identifier: AGPL-3.0-or-later
// Causal Nov5 integrated-dynamics capture: raw input, mono bend, static poly.
// A completed capture is not acceptance or a physical latency measurement.
#include "Pitch/PitchDetector.h"
#if __has_include("Pitch/InputDynamics.h")
#include "Pitch/InputDynamics.h"
#define FLYSNAPPER_HAS_DYNAMICS 1
#else
#define FLYSNAPPER_HAS_DYNAMICS 0
#endif
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
namespace fs=std::filesystem;
namespace {
template<class T,class=void>struct HasVoice:std::false_type{};
template<class T>struct HasVoice<T,std::void_t<decltype(std::declval<T>().voiceId)>>:std::true_type{};
template<class T,class=void>struct HasPolyBend:std::false_type{};
template<class T>struct HasPolyBend<T,std::void_t<decltype(std::declval<T>().polyBend)>>:std::true_type{};
template<class T>int voice(const T& event){if constexpr(HasVoice<T>::value)return event.voiceId;else return event.note;}
template<class T>void expression(T& settings,bool enabled){settings.monoBend=enabled;if constexpr(HasPolyBend<T>::value)settings.polyBend=false;settings.bendRangeSemitones=12;}
void need(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
template<class T,class=void>struct HasIntegratedDynamics:std::false_type{};
template<class T>struct HasIntegratedDynamics<T,std::void_t<decltype(std::declval<T>().inputDynamics)>>:std::true_type{};
template<class T,class=void>struct HasDynamicsMeter:std::false_type{};
template<class T>struct HasDynamicsMeter<T,std::void_t<decltype(std::declval<const T>().inputDynamicsMeter())>>:std::true_type{};
#if FLYSNAPPER_HAS_DYNAMICS
template<class T>void configureDynamics(T& config,const flysnapper::pitch::InputDynamics::Settings& settings){if constexpr(HasIntegratedDynamics<T>::value)config.inputDynamics=settings;else need(!settings.enabled,"Snapshot does not implement integrated post-conditioning dynamics");}
#endif
template<class T>void traceDynamics(const T& detector,std::ofstream& out,const std::string& name,int rate,int block,int sample){if constexpr(HasDynamicsMeter<T>::value){const auto m=detector.inputDynamicsMeter();out<<name<<','<<rate<<','<<block<<','<<sample<<','<<m.rawRmsDb<<','<<m.processedDb<<','<<m.gainDb<<','<<m.gateOpen<<','<<m.admissionOpen<<'\n';}}
std::vector<std::string> split(const std::string& s,char delim){std::vector<std::string> out;std::stringstream in(s);std::string part;while(std::getline(in,part,delim))out.push_back(part);return out;}
uint16_t u16(const std::vector<uint8_t>& b,size_t at){need(at+2<=b.size(),"Truncated WAV");return uint16_t(b[at])|(uint16_t(b[at+1])<<8);}
uint32_t u32(const std::vector<uint8_t>& b,size_t at){return uint32_t(u16(b,at))|(uint32_t(u16(b,at+2))<<16);}
struct Wave{int rate=0;std::vector<float> audio;};
Wave wave(const fs::path& path){
    std::ifstream in(path,std::ios::binary);need(bool(in),"Missing PCM: "+path.u8string());std::vector<uint8_t>b((std::istreambuf_iterator<char>(in)),{});
    need(b.size()>=12&&std::string(reinterpret_cast<const char*>(b.data()),4)=="RIFF"&&std::string(reinterpret_cast<const char*>(b.data()+8),4)=="WAVE","Invalid WAV");
    int format=0,channels=0,bits=0,rate=0;size_t data=0,length=0;
    for(size_t at=12;at+8<=b.size();){const uint32_t n=u32(b,at+4);need(at+8+uint64_t(n)<=b.size(),"WAV chunk outside file");const std::string tag(reinterpret_cast<const char*>(b.data()+at),4);
        if(tag=="fmt "){need(n>=16,"Short fmt");format=u16(b,at+8);channels=u16(b,at+10);rate=int(u32(b,at+12));bits=u16(b,at+22);}
        if(tag=="data"){data=at+8;length=n;}at+=8+n+(n&1);}
    need(format==1&&channels==1&&bits==16&&rate>0&&data&&!(length&1),"Expected mono PCM16");Wave out{rate,{}};out.audio.resize(length/2);
    for(size_t n=0;n<out.audio.size();++n)out.audio[n]=float(int16_t(u16(b,data+2*n)))/32768.f;
    return out;
}
std::vector<float> resample(const Wave& w,int rate){if(rate==w.rate)return w.audio;std::vector<float> out(size_t(std::llround(double(w.audio.size())*rate/w.rate)));
    for(size_t n=0;n<out.size();++n){const double at=double(n)*w.rate/rate;const auto a=std::min(size_t(at),w.audio.size()-1),b=std::min(a+1,w.audio.size()-1);out[n]=float(w.audio[a]+(w.audio[b]-w.audio[a])*(at-a));}return out;}
struct Case{std::string name,mode;fs::path file;bool bend=false;};
std::vector<Case> cases(const fs::path& folder){std::ifstream in(folder/"fixtures.tsv");need(bool(in),"Missing readiness inventory");std::vector<Case> out;std::string line;std::getline(in,line);
    while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;const auto f=split(line,'\t');need(f.size()>=4,"Malformed inventory");const auto path=fs::u8path(f[3]);need(!path.is_absolute()&&f[3].find("..") == std::string::npos,"Unsafe path");out.push_back({f[0],f[2],folder/path,f.size()>5&&f[5]=="1"});}return out;}
double quantile(std::vector<double> value,double q){if(value.empty())return 0;std::sort(value.begin(),value.end());return value[size_t(std::ceil(q*(value.size()-1)))];}
int run(const std::vector<fs::path>& args){try{
    need(args.size()>=3&&args.size()<=5,"GuitarEvaluationCapture <fixture-dir> <output-dir> [quick|full] [off|default|gate-only|compression-only]");const bool full=args.size()>=4&&args[3]=="full";const bool frontend=args.size()>=5&&args[4]!="off";need(!frontend||HasIntegratedDynamics<flysnapper::pitch::Settings>::value,"Snapshot lacks accepted integrated dynamics API");const auto fixtures=cases(args[1]);fs::create_directories(args[2]);
    std::ofstream events(args[2]/"events.csv"),metrics(args[2]/"capture.csv"),dynamicsTrace(args[2]/"dynamics-quiet-hum.csv");need(bool(events)&&bool(metrics)&&bool(dynamicsTrace),"Cannot create capture output");
    dynamicsTrace<<"case,rate,block,availability_sample,backend_reference_rms_db,processed_db,gain_db,gate_open,admission_open\n";dynamicsTrace<<std::setprecision(9);
    events<<"case,rate,block,sample,type,note,velocity,pitch,voice_id\n";events<<std::setprecision(9);
    metrics<<"case,rate,block,source_frames,frames,events,overflow,deadline_misses,cpu_p99_us,cpu_max_us,active_at_end,native_voice_ids,native_poly_bend,dynamics_enabled,poly_bend_enabled,native_integrated_dynamics\n";
    int runs=0;for(const auto& item:fixtures){if(item.mode!="mono")continue;const auto original=wave(item.file);for(int rate:full?std::vector<int>{44100,48000,96000}:std::vector<int>{48000})for(int block:full?std::vector<int>{32,64,128}:std::vector<int>{64}){
        auto detector=std::make_unique<flysnapper::pitch::PitchDetector>();detector->prepare(rate);flysnapper::pitch::Settings config;config.polyphonic=item.mode=="poly";expression(config,item.bend);
#if FLYSNAPPER_HAS_DYNAMICS
        flysnapper::pitch::InputDynamics::Settings dynamics;dynamics.enabled=frontend;
        if(args.size()>=5&&args[4]=="gate-only"){dynamics.compressorRatio=1;dynamics.compressorThresholdDb=0;dynamics.limiterCeilingDb=0;}
        if(args.size()>=5&&args[4]=="compression-only"){dynamics.gateThresholdDb=-96;dynamics.gateHysteresisDb=0;dynamics.expanderRatio=1;}
        configureDynamics(config,dynamics);
#endif
        if(std::getenv("PLAIN_MODE"))config.monoBend=false;detector->configure(config);
        auto input=resample(original,rate);const auto sourceFrames=input.size();
        // Every source is followed by supplied digital silence. An active
        // note at the end of a still-running hum file is not a hanging voice.
        input.resize(sourceFrames+size_t(std::llround(rate*.25)),0);
        flysnapper::pitch::Output output;int countEvents=0,overflow=0,misses=0;std::vector<double> times;
        for(int at=0;at<int(input.size());at+=block){const int n=std::min(block,int(input.size())-at);output.clear();const auto before=std::chrono::steady_clock::now();detector->process(input.data()+at,n,output);const auto after=std::chrono::steady_clock::now();
            if(frontend&&item.mode=="mono"&&item.name.rfind("quiet_hum_",0)==0)traceDynamics(*detector,dynamicsTrace,item.name,rate,block,at+n-1);
            const double us=std::chrono::duration<double,std::micro>(after-before).count();times.push_back(us);misses+=us>1e6*n/rate;overflow+=output.overflow;
            for(size_t k=0;k<output.size;++k){const auto& e=output.events[k];need(e.offset>=0&&e.offset<n,"Event outside supplied causal block");need(e.note>=0&&e.note<=127,"Invalid note");
                events<<item.name<<','<<rate<<','<<block<<','<<at+e.offset<<','<<(e.pitchChange?'P':e.on?'N':'F')<<','<<e.note<<','<<e.velocity<<',';
                if(e.pitchChange){need(std::isfinite(e.pitch),"Nonfinite pitch event");events<<e.pitch;}events<<','<<voice(e)<<'\n';++countEvents;
            }
        }
        metrics<<item.name<<','<<rate<<','<<block<<','<<sourceFrames<<','<<input.size()<<','<<countEvents<<','<<overflow<<','<<misses<<','<<quantile(times,.99)<<','<<quantile(times,1)<<','<<detector->status().activeNotes<<','<<HasVoice<flysnapper::pitch::Event>::value<<','<<HasPolyBend<flysnapper::pitch::Settings>::value<<','<<frontend<<",0,"<<HasIntegratedDynamics<flysnapper::pitch::Settings>::value<<'\n';++runs;
    }}
    std::cout<<"Captured "<<runs<<" causal integrated-dynamics Nov5 runs (poly bend disabled). Completion is not live-guitar or physical latency acceptance.\n";return 0;
}catch(const std::exception& e){std::cerr<<"Readiness capture failed: "<<e.what()<<'\n';return 1;}}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<fs::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return run(args);}
#else
int main(int argc,char** argv){std::vector<fs::path> args;for(int i=0;i<argc;++i)args.emplace_back(fs::u8path(argv[i]));return run(args);}
#endif
