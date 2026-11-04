// SPDX-License-Identifier: AGPL-3.0-or-later
// User-defect regressions: genuinely decaying notes, changing partial balance,
// false octaves, actual octave changes, independent damping and level invariance.
// These are fixed-note controls. Continuous bends have a separate processor test.
#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
int checks=0,failures=0;
void check(bool condition,const std::string& message){
    ++checks;if(!condition){++failures;std::cerr<<"FAIL "<<message<<'\n';}
}
void require(bool condition,const std::string& message){if(!condition)throw std::runtime_error(message);}
std::vector<std::string> split(const std::string& value,char separator){
    std::vector<std::string> result;std::stringstream in(value);std::string part;
    while(std::getline(in,part,separator))result.push_back(part);
    return result;
}
struct Truth{int note=0,on=0,off=0;};
struct Case{std::string name,family,mode;fs::path file;std::vector<Truth> truth;};
struct Event{bool on=false;int note=0,sample=0;};
struct Interval{int note=0,on=0,off=-1;bool used=false;};
struct PitchTruth{int sample=0;double pitch=0;};
std::vector<Case> readCases(const fs::path& folder){
    std::ifstream in(folder/"fixtures.tsv");require(bool(in),"Missing guitar fixture inventory");
    std::vector<Case> result;std::string line;std::getline(in,line);
    while(std::getline(in,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty())continue;
        const auto fields=split(line,'\t');require(fields.size()>=4,"Malformed guitar fixture row");
        const fs::path relative=fs::u8path(fields[3]);
        require(!relative.is_absolute()&&fields[3].find("..") == std::string::npos,"Unsafe guitar fixture path");
        Case c{fields[0],fields[1],fields[2],folder/relative,{}};
        require(c.mode=="mono"||c.mode=="poly","Unknown guitar mode");
        if(fields.size()>4)for(const auto& item:split(fields[4],',')){
            const auto value=split(item,':');require(value.size()==3,"Malformed guitar note truth");
            c.truth.push_back({std::stoi(value[0]),std::stoi(value[1]),std::stoi(value[2])});
        }
        result.push_back(std::move(c));
    }
    require(result.size()==55,"Independent guitar inventory must retain all55 mode cases");return result;
}
std::vector<float> readWave(const fs::path& file){
    std::ifstream in(file,std::ios::binary);require(bool(in),"Cannot open guitar PCM");
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
    const auto u16=[&](size_t at){require(at+2<=bytes.size(),"Truncated guitar WAV");return uint16_t(bytes[at])|(uint16_t(bytes[at+1])<<8);};
    const auto u32=[&](size_t at){return uint32_t(u16(at))|(uint32_t(u16(at+2))<<16);};
    require(bytes.size()>=12&&std::string(reinterpret_cast<const char*>(bytes.data()),4)=="RIFF"
            &&std::string(reinterpret_cast<const char*>(bytes.data()+8),4)=="WAVE","Invalid guitar WAV header");
    int format=0,channels=0,rate=0,bits=0;size_t data=0,length=0;
    for(size_t at=12;at+8<=bytes.size();){
        const uint32_t size=u32(at+4);require(at+8+uint64_t(size)<=bytes.size(),"Guitar WAV chunk outside file");
        const std::string kind(reinterpret_cast<const char*>(bytes.data()+at),4);
        if(kind=="fmt "){
            require(size>=16,"Short guitar WAV format");format=u16(at+8);channels=u16(at+10);rate=int(u32(at+12));bits=u16(at+22);
        }else if(kind=="data"){data=at+8;length=size;}
        at+=8+size+(size&1);
    }
    require(format==1&&channels==1&&rate==48000&&bits==16&&data&&!(length&1),"Guitar controls must remain48kHz mono PCM16");
    std::vector<float> audio(length/2);
    for(size_t i=0;i<audio.size();++i)audio[i]=float(int16_t(u16(data+i*2)))/32768.0f;
    return audio;
}
std::vector<Event> run(const std::vector<float>& audio,bool poly,int block){
    auto detector=std::make_unique<flysnapper::pitch::PitchDetector>();detector->prepare(48000);
    flysnapper::pitch::Settings settings;settings.polyphonic=poly;settings.monoBend=false;detector->configure(settings);
    std::vector<Event> result;flysnapper::pitch::Output out;
    for(int at=0;at<int(audio.size());at+=block){
        const int count=std::min(block,int(audio.size())-at);out.clear();detector->process(audio.data()+at,count,out);
        check(!out.overflow,"Guitar detector output overflow");
        for(size_t i=0;i<out.size;++i){const auto& e=out.events[i];
            check(e.offset>=0&&e.offset<count,"Guitar event offset outside supplied block");
            check(!e.pitchChange,"Disabled mono bend unexpectedly emitted fractional pitch");
            if(e.pitchChange)continue;
            result.push_back({e.on,e.note,at+e.offset});
        }
    }
    check(detector->status().activeNotes==0,"Guitar detector retained notes after source plus silence");return result;
}
void evaluate(const Case& c,const std::vector<Event>& events){
    std::vector<Interval> intervals;std::array<std::vector<size_t>,128> held;
    for(const auto& e:events){
        require(e.note>=0&&e.note<128,"Out-of-range guitar MIDI note");
        if(e.on){intervals.push_back({e.note,e.sample,-1,false});held[size_t(e.note)].push_back(intervals.size()-1);}
        else{
            check(!held[size_t(e.note)].empty(),c.name+" orphan NoteOff");
            if(!held[size_t(e.note)].empty()){
                const auto index=held[size_t(e.note)].front();held[size_t(e.note)].erase(held[size_t(e.note)].begin());intervals[index].off=e.sample;
            }
        }
    }
    check(intervals.size()==c.truth.size(),c.name+" extra/missing/retriggered NoteOn:expected="+std::to_string(c.truth.size())+" actual="+std::to_string(intervals.size()));
    for(const auto& truth:c.truth){
        Interval* chosen=nullptr;
        for(auto& interval:intervals)if(!interval.used&&interval.note==truth.note&&interval.on>=truth.on-2&&interval.on<truth.off){chosen=&interval;break;}
        check(chosen!=nullptr,c.name+" missing correct note"+std::to_string(truth.note));
        if(!chosen)continue;
        chosen->used=true;
        const double latency=1000.0*(chosen->on-truth.on)/48000;
        const double release=1000.0*(chosen->off-truth.off)/48000;
        std::cout<<c.name<<','<<truth.note<<','<<std::fixed<<std::setprecision(3)<<latency<<','<<release<<'\n';
        check(latency<=90,c.name+" acquisition exceeded90ms functional bound");
        check(chosen->off>=truth.off-960,c.name+" premature NoteOff before physical damping");
        check(chosen->off<=truth.off+4800,c.name+" NoteOff exceeded100ms after damping");
    }
    for(const auto& interval:intervals){
        check(interval.used,c.name+" false note"+std::to_string(interval.note));
        check(interval.off>=interval.on,c.name+" hanging NoteOn");
    }
}
std::map<std::string,std::vector<PitchTruth>> readPitchTruth(const fs::path& fixtures){
    std::ifstream in(fixtures/"pitch-curves.tsv");require(bool(in),"Missing continuous-pitch reference");
    std::map<std::string,std::vector<PitchTruth>> result;std::string line;std::getline(in,line);
    while(std::getline(in,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        const auto f=split(line,'\t');
        require(f.size()==3,"Malformed continuous-pitch reference");result[f[0]].push_back({std::stoi(f[1]),std::stod(f[2])});
    }
    require(result.size()==4,"All four independent continuous-pitch references must remain");
    for(const auto& c:result){require(c.second.size()>900,"Incomplete continuous-pitch reference");
        for(size_t i=1;i<c.second.size();++i)require(c.second[i].sample-c.second[i-1].sample==64,"Nonuniform continuous-pitch reference");}
    return result;
}
double truePitch(const std::vector<PitchTruth>& truth,int sample){
    if(sample<=truth.front().sample)return truth.front().pitch;
    if(sample>=truth.back().sample)return truth.back().pitch;
    const auto upper=std::upper_bound(truth.begin(),truth.end(),sample,[](int value,const PitchTruth& p){return value<p.sample;});
    const auto& a=*(upper-1);const auto& b=*upper;return a.pitch+(b.pitch-a.pitch)*(sample-a.sample)/double(b.sample-a.sample);
}
double quantile(std::vector<double> values,double q){require(!values.empty(),"No continuous-pitch observations");std::sort(values.begin(),values.end());return values[size_t(std::ceil(q*(values.size()-1)))];}
struct PitchEvent{int sample=0;float pitch=0;};
struct BendRun{std::vector<Event> notes;std::vector<PitchEvent> pitches;};
BendRun runBend(const Case& c,const std::vector<float>& audio,int range,int block){
    auto detector=std::make_unique<flysnapper::pitch::PitchDetector>();detector->prepare(48000);
    flysnapper::pitch::Settings settings;settings.monoBend=true;settings.bendRangeSemitones=float(range);detector->configure(settings);
    flysnapper::pitch::Output out;BendRun result;
    for(int pos=0;pos<int(audio.size());pos+=block){
        const int count=std::min(block,int(audio.size())-pos);out.clear();detector->process(audio.data()+pos,count,out);check(!out.overflow,c.name+" bend detector overflow");
        for(size_t i=0;i<out.size;++i){const auto& e=out.events[i];check(e.offset>=0&&e.offset<count,c.name+" bend event offset");
            if(e.pitchChange){
                check(std::isfinite(e.pitch)&&e.pitch>=28&&e.pitch<=100,c.name+" nonfinite/out-of-range continuous pitch");
                result.pitches.push_back({pos+e.offset,e.pitch});
            }else result.notes.push_back({e.on,e.note,pos+e.offset});
        }
    }
    check(detector->status().activeNotes==0,c.name+" bend voice hung after silence");return result;
}
void evaluateBend(const Case& c,const std::vector<PitchTruth>& truth,int range){
    const auto audio=readWave(c.file);const auto first=runBend(c,audio,range,64),second=runBend(c,audio,range,257);
    bool identical=first.notes.size()==second.notes.size()&&first.pitches.size()==second.pitches.size();
    if(identical){
        for(size_t i=0;i<first.notes.size();++i)identical=identical&&first.notes[i].on==second.notes[i].on&&first.notes[i].note==second.notes[i].note&&first.notes[i].sample==second.notes[i].sample;
        for(size_t i=0;i<first.pitches.size();++i)identical=identical&&first.pitches[i].sample==second.pitches[i].sample&&first.pitches[i].pitch==second.pitches[i].pitch;
    }
    check(identical,c.name+" note/pitch decisions changed with host block segmentation");evaluate(c,first.notes);
    check(first.pitches.size()>=20,c.name+" did not emit a genuinely changing continuous pitch");
    std::vector<double> errors,settled,moving;size_t pi=0,ni=0;float pitch=-1;bool active=false;
    for(int sample=c.truth.front().on;sample<c.truth.front().off;sample+=64){
        while(pi<first.pitches.size()&&first.pitches[pi].sample<=sample)pitch=first.pitches[pi++].pitch;
        while(ni<first.notes.size()&&first.notes[ni].sample<=sample)active=first.notes[ni++].on;
        if(!active||pitch<0)continue;
        const double error=100*std::abs(double(pitch)-truePitch(truth,sample));errors.push_back(error);
        // A settled segment must have been stable over the preceding32ms and
        // remain stable for32ms. Dynamic/fast-bend maxima are reported apart.
        std::array<double,5> neighborhood{};
        for(int i=0;i<5;++i)neighborhood[size_t(i)]=truePitch(truth,sample+(i-2)*768);
        const auto extremes=std::minmax_element(neighborhood.begin(),neighborhood.end());
        const double width=*extremes.second-*extremes.first;
        if(width<=.01)settled.push_back(error);else moving.push_back(error);
    }
    check(errors.size()>=800,c.name+" continuous-pitch coverage was incomplete");
    if(errors.empty())return;
    const double median=quantile(errors,.5),p95=quantile(errors,.95),maximum=quantile(errors,1);
    std::cout<<"BEND "<<c.name<<" range="<<range<<" observations="<<errors.size()<<" absCents[median/p95/max]="<<median<<'/'<<p95<<'/'<<maximum<<'\n';
    check(p95<=35,c.name+" bend P95 error exceeded35cents");
    check(maximum<=75,c.name+" bend transient error exceeded75cents");
    check(settled.size()>=30,c.name+" settled-pitch coverage was incomplete");
    check(!moving.empty(),c.name+" moving-pitch coverage was incomplete");
    if(!settled.empty()){
        const double maxSettled=quantile(settled,1);std::cout<<"BEND settledMaxCents "<<c.name<<' '<<maxSettled<<'\n';
        check(maxSettled<=20,c.name+" settled-pitch error exceeded20cents");
    }
    if(!moving.empty())std::cout<<"BEND movingMaxCents "<<c.name<<' '<<quantile(moving,1)<<'\n';
    int releaseEnd=int(audio.size());
    for(const auto& note:first.notes)if(!note.on)releaseEnd=std::min(releaseEnd,note.sample);
    double tailMaximum=0;
    for(const auto& p:first.pitches)if(p.sample>=c.truth.front().off&&p.sample<releaseEnd)
        tailMaximum=std::max(tailMaximum,100*std::abs(double(p.pitch)-truePitch(truth,c.truth.front().off)));
    std::cout<<"BEND releaseTailMaxCents "<<c.name<<' '<<tailMaximum<<'\n';
    check(tailMaximum<=35,c.name+" pitch wandered after physical damping but before NoteOff");
}
int mainRun(const fs::path& fixtures){try{
    const auto cases=readCases(fixtures);const auto pitchTruth=readPitchTruth(fixtures);int evaluated=0,bendCases=0;
    std::cout<<"case,note,first_correct_ms,release_after_source_ms\n";
    for(const auto& c:cases){
        if(c.family=="guitar-continuous-bend"){
            const auto name=c.name.substr(0,c.name.size()-5);const auto reference=pitchTruth.find(name);require(reference!=pitchTruth.end(),"Missing bend pitch reference");
            for(int range:{2,12})evaluateBend(c,reference->second,range);
            ++bendCases;continue;
        }
        const auto audio=readWave(c.file);const auto first=run(audio,c.mode=="poly",64),second=run(audio,c.mode=="poly",257);
        bool identical=first.size()==second.size();
        if(identical)for(size_t i=0;i<first.size();++i)identical=identical&&first[i].on==second[i].on&&first[i].note==second[i].note&&first[i].sample==second[i].sample;
        check(identical,c.name+" MIDI decisions changed with host block segmentation");evaluate(c,first);++evaluated;
    }
    check(evaluated==51&&bendCases==4,"All51 fixed-note controls and separate4 bend controls must remain inventoried");
    std::cout<<"FlysnapperGuitarQuality:"<<(failures?"FAIL":"PASS")<<" ("<<checks<<" checks,"<<evaluated<<" fixed-note cases x2 block sizes plus4 bend cases x2 ranges x2 block sizes;"<<failures<<" failures).\n";
    std::cout<<"Observed functional detector timing only; not physical5/10ms live latency certification. MIDI bend delivery uses a separate mandatory processor test.\n";
    return failures?1:0;
}catch(const std::exception& e){std::cerr<<"Guitar quality test error:"<<e.what()<<'\n';return 2;}}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){if(argc!=2)return 2;return mainRun(std::filesystem::path(argv[1]));}
#else
int main(int argc,char** argv){if(argc!=2)return 2;return mainRun(std::filesystem::u8path(argv[1]));}
#endif
