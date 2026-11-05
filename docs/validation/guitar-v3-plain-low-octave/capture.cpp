#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace flysnapper::pitch;
namespace fs=std::filesystem;
static uint16_t u16(const std::vector<uint8_t>& b,size_t at){if(at+2>b.size())throw std::runtime_error("truncated WAV");return uint16_t(b[at])|(uint16_t(b[at+1])<<8);}
static uint32_t u32(const std::vector<uint8_t>& b,size_t at){return uint32_t(u16(b,at))|(uint32_t(u16(b,at+2))<<16);}
static std::vector<float> wave(const fs::path& path){
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("missing PCM");
    std::vector<uint8_t>b((std::istreambuf_iterator<char>(file)),{});
    if(b.size()<12||std::string(reinterpret_cast<const char*>(b.data()),4)!="RIFF"||std::string(reinterpret_cast<const char*>(b.data()+8),4)!="WAVE")throw std::runtime_error("invalid WAV");
    int format=0,channels=0,bits=0,rate=0;size_t atData=0,bytes=0;
    for(size_t at=12;at+8<=b.size();){const uint32_t n=u32(b,at+4);if(at+8+uint64_t(n)>b.size())throw std::runtime_error("bad WAV chunk");
        const std::string tag(reinterpret_cast<const char*>(b.data()+at),4);
        if(tag=="fmt "){format=u16(b,at+8);channels=u16(b,at+10);rate=int(u32(b,at+12));bits=u16(b,at+22);}
        if(tag=="data"){atData=at+8;bytes=n;}at+=8+n+(n&1);
    }
    if(format!=1||channels!=1||bits!=16||rate!=48000||!atData||bytes%2)throw std::runtime_error("expected mono48k PCM16");
    std::vector<float>result(bytes/2);for(size_t i=0;i<result.size();++i)result[i]=float(int16_t(u16(b,atData+2*i)))/32768.f;return result;
}
int main(int argc,char**argv){try{
    if(argc!=3)throw std::runtime_error("QuietAliasCapture <fixture-directory> <output-directory>");
    const fs::path fixtures=argv[1],out=argv[2];fs::create_directories(out);
    std::ofstream events(out/"events.csv"),status(out/"status.csv"),metrics(out/"capture.csv");
    events<<"case,mono_bend,dynamics_enabled,sample,type,note,velocity,pitch,voice_id\n";
    status<<"case,mono_bend,dynamics_enabled,sample,level_db,confidence,active,strongest,input_present,pitch_classes,pitch,raw_rms_db,processed_db,gain_db,gate_open,admission_open\n";
    metrics<<"case,note,gain,mono_bend,dynamics_enabled,rate,block,bend_range,source_frames,frames,events,overflow,active_at_end\n";
    events<<std::setprecision(10);status<<std::setprecision(10);
    int count=0;
    for(int note:{31,40,52})for(int gain:{1,8})for(bool bend:{false,true})for(bool enabled:{false,true}){
        const std::string name="quiet_hum_"+std::to_string(note)+"_gain"+std::to_string(gain);
        auto input=wave(fixtures/"pcm"/(name+".wav"));const auto sourceFrames=input.size();input.resize(sourceFrames+12000,0);
        auto detector=std::make_unique<PitchDetector>();detector->prepare(48000);Settings settings;
        settings.monoBend=bend;settings.inputDynamics.enabled=enabled;detector->configure(settings);
        Output output;int eventCount=0,overflow=0;
        for(int at=0;at<int(input.size());at+=64){const int n=std::min(64,int(input.size())-at);output.clear();detector->process(input.data()+at,n,output);overflow+=output.overflow;
            for(size_t k=0;k<output.size;++k){const auto&e=output.events[k];if(e.offset<0||e.offset>=n)throw std::runtime_error("event offset outside input");
                events<<name<<','<<bend<<','<<enabled<<','<<at+e.offset<<','<<(e.pitchChange?'P':e.on?'N':'F')<<','<<e.note<<','<<e.velocity<<',';
                if(e.pitchChange)events<<e.pitch;events<<','<<e.voiceId<<'\n';++eventCount;
            }
            const auto s=detector->status();const auto m=detector->inputDynamicsMeter();
            status<<name<<','<<bend<<','<<enabled<<','<<at+n-1<<','<<s.levelDb<<','<<s.confidence<<','<<s.activeNotes<<','<<s.strongestNote<<','<<s.inputPresent<<','<<s.activePitchClasses<<','<<s.continuousPitch<<','<<m.rawRmsDb<<','<<m.processedDb<<','<<m.gainDb<<','<<m.gateOpen<<','<<m.admissionOpen<<'\n';
        }
        metrics<<name<<','<<note<<','<<gain<<','<<bend<<','<<enabled<<",48000,64,"<<settings.bendRangeSemitones<<','<<sourceFrames<<','<<input.size()<<','<<eventCount<<','<<overflow<<','<<detector->status().activeNotes<<'\n';++count;
    }
    std::cout<<"Completed "<<count<<" frozen Native3 quiet-source captures.\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
