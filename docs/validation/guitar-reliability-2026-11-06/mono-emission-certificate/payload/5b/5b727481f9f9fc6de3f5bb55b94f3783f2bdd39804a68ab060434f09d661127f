// SPDX-License-Identifier: AGPL-3.0-or-later
// Independent fixture evaluator. This reports observations; successful execution
// is not acceptance of the product's physical 5/10ms end-to-end targets.
#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <utility>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace {
bool integratedDynamicsEnabled=false;
struct Truth {int note;int on,off;};
struct Fixture {std::string name,family,mode;fs::path file;std::vector<Truth> truth;};
struct Wave {int rate;std::vector<float> audio;};
struct Event {bool on;int note,velocity,sample;};
struct Interval {int note,on,off;bool matched=false;};
struct Result {
    std::string name,family,mode;int rate=0,block=0,expected=0,matched=0,missed=0,falseOns=0,octaves=0,wrongFirst=0,earlyOffs=0,hung=0,overflow=0,deadlineMisses=0;
    std::vector<double> latency,release,blockTimes;
    std::vector<Event> events;
};
std::vector<std::string> split(const std::string& s,char c){std::vector<std::string> out;std::stringstream in(s);std::string token;while(std::getline(in,token,c))out.push_back(token);return out;}
uint16_t u16(const std::vector<uint8_t>& b,size_t i){if(i+2>b.size())throw std::runtime_error("truncated WAV");return uint16_t(b[i])|(uint16_t(b[i+1])<<8);}
uint32_t u32(const std::vector<uint8_t>& b,size_t i){return uint32_t(u16(b,i))|(uint32_t(u16(b,i+2))<<16);}
Wave readWave(const fs::path& path){
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot open PCM fixture: "+path.u8string());
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)),{});
    if(b.size()<12||std::string(reinterpret_cast<const char*>(b.data()),4)!="RIFF"||std::string(reinterpret_cast<const char*>(b.data()+8),4)!="WAVE")throw std::runtime_error("Not RIFF/WAVE");
    uint16_t format=0,channels=0,bits=0;uint32_t rate=0;size_t data=0,length=0;
    for(size_t at=12;at+8<=b.size();){uint32_t n=u32(b,at+4);if(at+8+uint64_t(n)>b.size())throw std::runtime_error("WAV chunk outside file");
        auto type=std::string(reinterpret_cast<const char*>(b.data()+at),4);
        if(type=="fmt "){if(n<16)throw std::runtime_error("bad fmt");format=u16(b,at+8);channels=u16(b,at+10);rate=u32(b,at+12);bits=u16(b,at+22);}
        if(type=="data"){data=at+8;length=n;}at+=8+n+(n&1);
    }
    if(format!=1||channels!=1||bits!=16||!rate||!data)throw std::runtime_error("Fixture must be mono PCM16");
    Wave out{int(rate),{}};out.audio.reserve(length/2);
    for(size_t i=0;i<length;i+=2)out.audio.push_back(float(int16_t(u16(b,data+i)))/32768.0f);
    return out;
}
std::vector<Fixture> readFixtures(const fs::path& folder){
    std::ifstream in(folder/"fixtures.tsv");if(!in)throw std::runtime_error("Missing fixtures.tsv");
    std::vector<Fixture> out;std::string line;std::getline(in,line);
    while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
        auto f=split(line,'\t');if(f.size()<4)throw std::runtime_error("Malformed fixture inventory");
        Fixture x{f[0],f[1],f[2],folder/fs::u8path(f[3]),{}};
        if(f.size()>4&&!f[4].empty())for(const auto& value:split(f[4],',')){auto t=split(value,':');if(t.size()!=3)throw std::runtime_error("Malformed truth");x.truth.push_back({std::stoi(t[0]),std::stoi(t[1]),std::stoi(t[2])});}
        out.push_back(std::move(x));
    }return out;
}
std::vector<float> resample(const Wave& w,int rate){
    if(w.rate==rate)return w.audio;
    std::vector<float> out(size_t(std::llround(double(w.audio.size())*rate/w.rate)));
    for(size_t i=0;i<out.size();++i){double pos=double(i)*w.rate/rate;size_t p=std::min(size_t(pos),w.audio.size()-1);size_t q=std::min(p+1,w.audio.size()-1);out[i]=float(w.audio[p]+(w.audio[q]-w.audio[p])*(pos-double(p)));}
    return out;
}
bool activeAt(const Truth& t,int sample,int slack=0){return sample>=t.on-slack&&sample<=t.off+slack;}
double quantile(std::vector<double> v,double q){if(v.empty())return std::numeric_limits<double>::quiet_NaN();std::sort(v.begin(),v.end());return v[size_t(std::ceil(q*double(v.size()-1)))];}
Result evaluate(const Fixture& fixture,const Wave& w,int rate,int block){
    Result result;result.name=fixture.name;result.family=fixture.family;result.mode=fixture.mode;result.rate=rate;result.block=block;result.expected=int(fixture.truth.size());
    auto audio=resample(w,rate);auto truth=fixture.truth;for(auto& t:truth){t.on=int(std::llround(double(t.on)*rate/w.rate));t.off=int(std::llround(double(t.off)*rate/w.rate));}
    // Detector construction/table preparation and fixture IO are excluded from the processing timings.
    flysnapper::pitch::PitchDetector detector;detector.prepare(rate);flysnapper::pitch::Settings settings;settings.polyphonic=fixture.mode=="poly";settings.inputDynamics.enabled=integratedDynamicsEnabled;detector.configure(settings);
    flysnapper::pitch::Output output;
    for(int pos=0;pos<int(audio.size());pos+=block){int count=std::min(block,int(audio.size())-pos);output.clear();
        auto start=std::chrono::steady_clock::now();detector.process(audio.data()+pos,count,output);auto stop=std::chrono::steady_clock::now();
        double us=std::chrono::duration<double,std::micro>(stop-start).count();result.blockTimes.push_back(us);if(us>1e6*count/rate)++result.deadlineMisses;
        if(output.overflow)++result.overflow;
        for(size_t j=0;j<output.size;++j){const auto& e=output.events[j];if(e.offset<0||e.offset>=count)throw std::runtime_error("Detector emitted offset outside supplied block");result.events.push_back({e.on,e.note,e.velocity,pos+e.offset});}
    }
    std::vector<Interval> intervals;std::array<std::vector<size_t>,128> held;
    for(const auto& e:result.events){if(e.note<0||e.note>127)throw std::runtime_error("Invalid emitted MIDI note");
        if(e.on){intervals.push_back({e.note,e.sample,int(audio.size()),false});held[size_t(e.note)].push_back(intervals.size()-1);}
        else if(!held[size_t(e.note)].empty()){auto index=held[size_t(e.note)].front();held[size_t(e.note)].erase(held[size_t(e.note)].begin());intervals[index].off=e.sample;}
    }
    for(const auto& x:held)result.hung+=int(x.size());
    std::vector<int> chosen(truth.size(),-1);
    // Match first *usable* correct onset: at least20ms or the remaining actual note length.
    // Short correct guesses cannot become an artificially good low-latency score.
    for(size_t i=0;i<truth.size();++i){const auto& t=truth[i];for(size_t j=0;j<intervals.size();++j){auto& interval=intervals[j];
            int usable=std::min(int(std::llround(.020*rate)),t.off-t.on);
            if(!interval.matched&&interval.note==t.note&&activeAt(t,interval.on,2)&&std::min(interval.off,t.off)-std::max(interval.on,t.on)>=usable){chosen[i]=int(j);interval.matched=true;break;}}
        if(chosen[i]>=0){auto& interval=intervals[size_t(chosen[i])];++result.matched;result.latency.push_back(1000.0*(interval.on-t.on)/rate);
            if(interval.off<t.off-int(.01*rate))++result.earlyOffs;else result.release.push_back(1000.0*(interval.off-t.off)/rate);
        }else ++result.missed;
    }
    for(const auto& interval:intervals)if(!interval.matched){++result.falseOns;
        bool octave=false;for(const auto& t:truth)if(activeAt(t,interval.on)){int diff=std::abs(t.note-interval.note);if(diff==12||diff==24||diff==36)octave=true;}
        if(octave)++result.octaves;
    }
    for(size_t i=0;i<truth.size();++i){const auto& t=truth[i];int correct=chosen[i]<0?t.off:intervals[size_t(chosen[i])].on;
        bool wrong=false;for(const auto& interval:intervals)if(interval.on>=t.on&&interval.on<correct){bool legitimate=false;for(const auto& another:truth)if(another.note==interval.note&&activeAt(another,interval.on,2))legitimate=true;if(!legitimate){wrong=true;break;}}
        if(wrong)++result.wrongFirst;
    }
    return result;
}
void number(std::ostream& out,double n){if(std::isfinite(n))out<<std::fixed<<std::setprecision(6)<<n;}
void writeResults(const fs::path& folder,const std::vector<Result>& results){
    fs::create_directories(folder);std::ofstream csv(folder/"audio-cases.csv");
    csv<<"case,family,mode,rate,block,expected,matched,missed,false_note_ons,octave_errors,wrong_before_correct,early_offs,hung,overflow,latency_median_ms,latency_p95_ms,latency_p99_ms,latency_max_ms,release_median_ms,release_p95_ms,cpu_median_us,cpu_p95_us,cpu_p99_us,cpu_max_us,deadline_misses\n";
    std::ofstream events(folder/"audio-events.csv");events<<"case,rate,block,on,note,velocity,sample,time_ms\n";
    for(const auto& r:results){csv<<r.name<<','<<r.family<<','<<r.mode<<','<<r.rate<<','<<r.block<<','<<r.expected<<','<<r.matched<<','<<r.missed<<','<<r.falseOns<<','<<r.octaves<<','<<r.wrongFirst<<','<<r.earlyOffs<<','<<r.hung<<','<<r.overflow<<',';
        for(auto [v,q]:std::array<std::pair<const std::vector<double>*,double>,10>{{{&r.latency,.5},{&r.latency,.95},{&r.latency,.99},{&r.latency,1},{&r.release,.5},{&r.release,.95},{&r.blockTimes,.5},{&r.blockTimes,.95},{&r.blockTimes,.99},{&r.blockTimes,1}}}){number(csv,quantile(*v,q));csv<<',';}csv<<r.deadlineMisses<<'\n';
        for(const auto& e:r.events){events<<r.name<<','<<r.rate<<','<<r.block<<','<<e.on<<','<<e.note<<','<<e.velocity<<','<<e.sample<<',';number(events,1000.0*e.sample/r.rate);events<<'\n';}
    }
    std::ofstream summary(folder/"audio-summary.txt");summary<<"MEASUREMENTS, NOT PRODUCT ACCEPTANCE OR PHYSICAL END-TO-END LATENCY\n20ms minimum useful emitted interval; original PCM waveforms and independent annotations.\n";
    for(const auto& mode:{std::string("mono"),std::string("poly")}){int expected=0,matched=0,missed=0,falseOns=0,octave=0,wrong=0,early=0,hung=0,overflow=0,deadlines=0;std::vector<double> latency,cpu;
        for(const auto& r:results)if(r.mode==mode){expected+=r.expected;matched+=r.matched;missed+=r.missed;falseOns+=r.falseOns;octave+=r.octaves;wrong+=r.wrongFirst;early+=r.earlyOffs;hung+=r.hung;overflow+=r.overflow;deadlines+=r.deadlineMisses;latency.insert(latency.end(),r.latency.begin(),r.latency.end());cpu.insert(cpu.end(),r.blockTimes.begin(),r.blockTimes.end());}
        summary<<mode<<": expected="<<expected<<" matched="<<matched<<" missed="<<missed<<" falseOns="<<falseOns<<" octaveErrors="<<octave<<" wrongBeforeCorrect="<<wrong<<" earlyOffs="<<early<<" hung="<<hung<<" overflow="<<overflow<<" deadlineMisses="<<deadlines<<" latencyMs[median/p95/p99/max]=";
        for(double q:{.5,.95,.99,1.0}){number(summary,quantile(latency,q));summary<<' ';}summary<<" cpuUs[median/p95/p99/max]=";for(double q:{.5,.95,.99,1.0}){number(summary,quantile(cpu,q));summary<<' ';}summary<<'\n';
    }
    std::cout<<"Independent audio measurements written to "<<folder.u8string()<<" ("<<results.size()<<" case/rate/block runs).\n";
}
}
int run(const std::vector<fs::path>& args){try{
    if(args.size()<3||args.size()>5){std::cerr<<"usage: AudioValidation <fixtures-directory> <output-directory> [full]\n";return 2;}
    integratedDynamicsEnabled=args.size()>=5&&args[4]=="default";auto fixtures=readFixtures(args[1]);std::vector<Result> results;
    const bool full=args.size()==4&&args[3].u8string()=="full";std::vector<int> rates=full?std::vector<int>{44100,48000,96000}:std::vector<int>{48000};std::vector<int> blocks=full?std::vector<int>{32,64,128}:std::vector<int>{64};
    for(const auto& f:fixtures){if(f.mode!="mono")continue;auto wave=readWave(f.file);for(int rate:rates)for(int block:blocks)results.push_back(evaluate(f,wave,rate,block));}
    writeResults(args[2],results);return 0;
}catch(const std::exception& error){std::cerr<<"AudioValidation failed: "<<error.what()<<'\n';return 1;}}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<fs::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return run(args);}
#else
int main(int argc,char** argv){std::vector<fs::path> args;for(int i=0;i<argc;++i)args.emplace_back(fs::u8path(argv[i]));return run(args);}
#endif
