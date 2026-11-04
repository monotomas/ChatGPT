// SPDX-License-Identifier: AGPL-3.0-or-later
// Independent oracle-seeded tracking capture. No note acquisition is tested.
// Private access is restricted to one truthful initial state injection; phase,
// model coefficients and subsequent tracking are never injected or repaired.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <time.h>
#define private public
#include "Pitch/PolyphonicTracker.h"
#undef private
#include "Pitch/PolyphonicTracker.cpp"

namespace fs=std::filesystem;
using flysnapper::pitch::PolyphonicTracker;
namespace {
constexpr int analysisRate=12000,hop=32,maximum=512;
void need(bool test,const std::string& text){if(!test)throw std::runtime_error(text);}
std::vector<std::string> split(const std::string& text){
    std::vector<std::string> result;std::stringstream stream(text);std::string part;
    while(std::getline(stream,part,'\t'))result.push_back(part);
    return result;
}
int integer(const std::string& value){
    size_t end=0;const auto n=std::stoll(value,&end);
    need(end==value.size()&&n>=std::numeric_limits<int>::min()&&n<=std::numeric_limits<int>::max(),"Invalid integer");
    return int(n);
}
double number(const std::string& value){
    size_t end=0;const auto n=std::stod(value,&end);need(end==value.size()&&std::isfinite(n),"Invalid finite number");return n;
}
std::vector<std::vector<std::string>> rows(const fs::path& file,const std::string& header,size_t width){
    std::ifstream input(file);need(bool(input),"Cannot read "+file.string());std::string line;
    need(bool(std::getline(input,line)),"Missing TSV header");if(!line.empty()&&line.back()=='\r')line.pop_back();
    need(line==header,"Unexpected TSV header in "+file.string());std::vector<std::vector<std::string>> result;
    while(std::getline(input,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty())continue;
        const auto fields=split(line);need(fields.size()==width,"Unexpected TSV width in "+file.string());result.push_back(fields);
    }
    return result;
}
struct Seed{int slot=0,id=0,anchor=0;double pitch=0;};
struct Case{std::string name,file,observation;int frames=0,seedSample=0,warmupEnd=0;std::vector<Seed> seeds;};
std::vector<Case> inventory(const fs::path& folder){
    std::vector<Case> result;std::map<std::string,size_t> names;
    for(const auto& row:rows(folder/"cases.tsv","case\tfile\tframes\tobservation\tseed_sample\twarmup_end",6)){
        Case item{row[0],row[1],row[3],integer(row[2]),integer(row[4]),integer(row[5]),{}};
        need(!item.name.empty()&&item.name.find_first_of(",\r\n")==std::string::npos,"Unsafe case name");
        need(names.emplace(item.name,result.size()).second,"Duplicate case");
        const fs::path path=fs::u8path(item.file);need(!path.is_absolute(),"Absolute PCM path");
        for(const auto& part:path)need(part!="..","Parent-relative PCM path");
        need(item.frames>0&&item.seedSample>=maximum-1&&item.seedSample%hop==hop-1&&item.seedSample<item.frames,"Invalid seed timing");
        need(item.warmupEnd>item.seedSample&&item.warmupEnd<item.frames,"Invalid warmup timing");
        need(item.observation=="full512"||item.observation=="varied","Unknown observation mode");result.push_back(item);
    }
    for(const auto& row:rows(folder/"seeds.tsv","case\tslot\tvoice_id\tanchor\tpitch",5)){
        auto found=names.find(row[0]);need(found!=names.end(),"Seed refers to unknown case");
        Seed seed{integer(row[1]),integer(row[2]),integer(row[3]),number(row[4])};
        need(seed.slot>=0&&seed.slot<6&&seed.id==seed.slot,"Prototype voice identity must equal its physical state slot");
        need(seed.anchor>=0&&seed.anchor<=127&&seed.pitch>=0&&seed.pitch<=127,"Invalid oracle note/pitch");
        auto& list=result[found->second].seeds;
        need(std::none_of(list.begin(),list.end(),[&](const Seed& other){return other.slot==seed.slot;}),"Duplicate oracle state slot");list.push_back(seed);
    }
    for(const auto& item:result)need(!item.seeds.empty(),"Case has no oracle seed");
    return result;
}
std::vector<float> wave(const fs::path& path,int expected){
    std::ifstream input(path,std::ios::binary);need(bool(input),"Missing PCM "+path.string());
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    need(bytes.size()==size_t(expected)*4,"PCM length does not match sealed inventory");
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559,"IEEE binary32 float required");
    std::vector<float> output(size_t(expected),0);
    for(size_t j=0;j<output.size();++j){const auto at=4*j;
        const std::uint32_t bits=std::uint32_t(bytes[at])|(std::uint32_t(bytes[at+1])<<8)|(std::uint32_t(bytes[at+2])<<16)|(std::uint32_t(bytes[at+3])<<24);
        std::memcpy(&output[j],&bits,4);need(std::isfinite(output[j]),"Nonfinite source PCM");
    }
    return output;
}
double cpuMicroseconds(){
#ifdef CLOCK_THREAD_CPUTIME_ID
    timespec stamp{};need(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&stamp)==0,"Cannot read thread CPU clock");
    return double(stamp.tv_sec)*1e6+double(stamp.tv_nsec)*.001;
#else
    return -1;
#endif
}
void capture(const Case& item,const fs::path& folder,std::ostream& frames,std::ostream& voices){
    const auto audio=wave(folder/fs::u8path(item.file),item.frames);
    PolyphonicTracker tracker;tracker.prepare(analysisRate);PolyphonicTracker::Settings settings;
    settings.newNoteThresholdDb=-90;settings.earlyAcquisition=false;settings.legacySeeds=false;settings.externalSeedMode=true;
    std::array<float,maximum> ring{},observation{};int write=0,filled=0;bool seeded=false;
    for(int sample=0;sample<item.frames;++sample){
        ring[size_t(write)]=audio[size_t(sample)];write=(write+1)%maximum;filled=std::min(maximum,filled+1);
        if(sample%hop!=hop-1||filled<128)continue;
        int count=filled-filled%hop;
        if(item.observation=="varied"&&sample>=analysisRate/2){
            constexpr std::array<int,5> sizes{256,320,384,448,512};count=std::min(count,sizes[size_t((sample/hop)%5)]);
        }
        for(int j=0;j<count;++j)observation[size_t(j)]=ring[size_t((write-count+j+maximum)%maximum)];
        if(sample==item.seedSample){
            need(!seeded,"Oracle seeded more than once");
            for(const auto& seed:item.seeds){auto& state=tracker.data->states[size_t(seed.slot)];state={};state.view.active=true;state.view.id=std::uint32_t(seed.id);state.view.anchorNote=seed.anchor;state.view.pitch=float(seed.pitch);state.view.frequency=float(440*std::pow(2.,(seed.pitch-69)/12.));}
            seeded=true;
        }
        PolyphonicTracker::Frame output;
        const auto before=std::chrono::steady_clock::now();const double cpuBefore=cpuMicroseconds();
        tracker.analyse(observation.data(),count,hop,settings,output);
        const double cpuAfter=cpuMicroseconds();const auto after=std::chrono::steady_clock::now();
        const double wall=std::chrono::duration<double,std::micro>(after-before).count(),cpu=cpuBefore<0?-1:cpuAfter-cpuBefore;
        need(output.count>=0&&output.count<=6,"Invalid output voice count");
        frames<<item.name<<','<<sample<<','<<count<<','<<output.count<<','<<wall<<','<<cpu<<'\n';
        int active=0;
        for(size_t slot=0;slot<output.voices.size();++slot){const auto& voice=output.voices[slot];if(!voice.active)continue;++active;
            need(std::isfinite(voice.pitch)&&std::isfinite(voice.level)&&std::isfinite(voice.confidence),"Nonfinite active voice");
            voices<<item.name<<','<<sample<<','<<slot<<','<<voice.id<<','<<voice.anchorNote<<','<<voice.pitch<<','<<voice.level<<','<<voice.confidence<<','<<voice.onset<<','<<voice.retrigger<<','<<voice.phaseObservable<<'\n';
        }
        need(active==output.count,"Active states disagree with reported count");
    }
    need(seeded,"Oracle seed point was not reached");
}
int run(int argc,char** argv){try{
    need(argc==3,"Usage: PolyOracleCapture <fixture-directory> <output-directory>");
    const fs::path folder=fs::u8path(argv[1]),output=fs::u8path(argv[2]);
    need(!fs::exists(output)||fs::is_empty(output),"Refuse existing nonempty capture directory");
    const auto cases=inventory(folder);fs::create_directories(output);
    std::ofstream frames(output/"frames.csv"),voices(output/"voices.csv");need(bool(frames)&&bool(voices),"Cannot create raw capture");
    frames<<"case,sample,count_frames,output_count,wall_us,cpu_us\n";
    voices<<"case,sample,slot,id,anchor,pitch,level,confidence,onset,retrigger,phase_observable\n";
    frames<<std::setprecision(10);voices<<std::setprecision(10);
    for(const auto& item:cases){capture(item,folder,frames,voices);std::cout<<"Captured "<<item.name<<'\n';}
    need(bool(frames)&&bool(voices),"Failed writing raw capture");
    std::cout<<"Oracle-seeded tracking capture complete; acquisition and musical readiness were not tested.\n";return 0;
}catch(const std::exception& error){std::cerr<<"Oracle capture failed: "<<error.what()<<'\n';return 1;}}
}
int main(int argc,char** argv){return run(argc,argv);}
