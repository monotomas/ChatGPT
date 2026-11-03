// SPDX-License-Identifier: AGPL-3.0-or-later
// User-defect regressions: genuinely decaying notes, changing partial balance,
// false octaves, actual octave changes, independent damping and level invariance.
// These are fixed-note controls. Continuous bends have a separate processor test.
#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <array>
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
    while(std::getline(in,part,separator))result.push_back(part);return result;
}
struct Truth{int note=0,on=0,off=0;};
struct Case{std::string name,family,mode;fs::path file;std::vector<Truth> truth;};
struct Event{bool on=false;int note=0,sample=0;};
struct Interval{int note=0,on=0,off=-1;bool used=false;};
std::vector<Case> readCases(const fs::path& folder){
    std::ifstream in(folder/"fixtures.tsv");require(bool(in),"Missing guitar fixture inventory");
    std::vector<Case> result;std::string line;std::getline(in,line);
    while(std::getline(in,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
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
    flysnapper::pitch::Settings settings;settings.polyphonic=poly;detector->configure(settings);
    std::vector<Event> result;flysnapper::pitch::Output out;
    for(int at=0;at<int(audio.size());at+=block){
        const int count=std::min(block,int(audio.size())-at);out.clear();detector->process(audio.data()+at,count,out);
        check(!out.overflow,"Guitar detector output overflow");
        for(size_t i=0;i<out.size;++i){const auto& e=out.events[i];
            check(e.offset>=0&&e.offset<count,"Guitar event offset outside supplied block");
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
        if(!chosen)continue;chosen->used=true;
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
int mainRun(const fs::path& fixtures){try{
    const auto cases=readCases(fixtures);int evaluated=0,bendCases=0;
    std::cout<<"case,note,first_correct_ms,release_after_source_ms\n";
    for(const auto& c:cases){
        if(c.family=="guitar-continuous-bend"){++bendCases;continue;}
        const auto audio=readWave(c.file);const auto first=run(audio,c.mode=="poly",64),second=run(audio,c.mode=="poly",257);
        bool identical=first.size()==second.size();
        if(identical)for(size_t i=0;i<first.size();++i)identical=identical&&first[i].on==second[i].on&&first[i].note==second[i].note&&first[i].sample==second[i].sample;
        check(identical,c.name+" MIDI decisions changed with host block segmentation");evaluate(c,first);++evaluated;
    }
    check(evaluated==51&&bendCases==4,"All51 fixed-note controls and separate4 bend controls must remain inventoried");
    std::cout<<"FlysnapperGuitarQuality:"<<(failures?"FAIL":"PASS")<<" ("<<checks<<" checks,"<<evaluated<<" fixed-note cases x2 block sizes;"<<failures<<" failures).\n";
    std::cout<<"Observed functional detector timing only; not physical5/10ms live latency certification. Continuous bend uses a separate mandatory processor test.\n";
    return failures?1:0;
}catch(const std::exception& e){std::cerr<<"Guitar quality test error:"<<e.what()<<'\n';return 2;}}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){if(argc!=2)return 2;return mainRun(std::filesystem::path(argv[1]));}
#else
int main(int argc,char** argv){if(argc!=2)return 2;return mainRun(std::filesystem::u8path(argv[1]));}
#endif
