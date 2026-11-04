// SPDX-License-Identifier: AGPL-3.0-or-later
// Diagnostic only. Not linked into the plugin or musical acceptance runner.
#include "Pitch/NoiseConditioner.h"
#include "Pitch/YinEstimator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
std::vector<float> readWave(const std::string& path){
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot read fixture: "+path);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});
    const auto u16=[&](size_t at){if(at+2>bytes.size())throw std::runtime_error("Truncated WAV");return unsigned(bytes[at])+(unsigned(bytes[at+1])<<8);};
    const auto u32=[&](size_t at){return u16(at)+(u16(at+2)<<16);};
    if(bytes.size()<44||std::string((const char*)bytes.data(),4)!="RIFF"
        ||std::string((const char*)bytes.data()+8,4)!="WAVE")throw std::runtime_error("Expected RIFF/WAV");
    if(size_t(u32(4))+8>bytes.size())throw std::runtime_error("Truncated RIFF/WAV");
    bool format=false;size_t start=0,size=0;
    for(size_t at=12;at+8<=bytes.size();){const auto count=u32(at+4);if(at+8+count>bytes.size())throw std::runtime_error("Truncated WAV chunk");
        const std::string name((const char*)bytes.data()+at,4);
        if(name=="fmt "){format=count>=16&&u16(at+8)==1&&u16(at+10)==1&&u32(at+12)==48000
            &&u32(at+16)==96000&&u16(at+20)==2&&u16(at+22)==16;}
        if(name=="data"){start=at+8;size=count;break;}at+=8+count+(count&1);
    }
    if(!format||!start||size%2)throw std::runtime_error("Expected mono48kHz16-bit PCM: "+path);
    // Frozen old frontend:12Hz DC blocker, four-pole5040Hz low pass,12kHz analysis.
    std::vector<float> result;result.reserve(size/8);
    double dcX=0,dcY=0,low1=0,low2=0,low3=0,low4=0;
    const double pole=std::exp(-2*pi*12/48000),low=1-std::exp(-2*pi*5040/48000);
    for(size_t i=0;i<size/2;++i){const double x=std::int16_t(u16(start+2*i))/32768.;
        const double d=x-dcX+pole*dcY;dcX=x;dcY=d;
        low1+=low*(d-low1);low2+=low*(low1-low2);low3+=low*(low2-low3);low4+=low*(low3-low4);
        if(i%4==3)result.push_back(float(low4));
    }
    return result;
}
struct Random {std::uint32_t value=1234567;double next(){value=value*1664525+1013904223;return (double(value)/4294967296.-.5)*2;}};
}

int main(int argc,char** argv){try{
    if(argc<6)throw std::runtime_error("Usage: probe fixtures.tsv warmup_ms prime0or1 trim0or1 tolerance_cents [hum_amplitude=.03] [white_amplitude=.000025] [single_partial_rescue0or1=0]");
    std::ifstream fixtures(argv[1]);if(!fixtures)throw std::runtime_error("Cannot read fixture inventory");
    const double warmupMs=std::stod(argv[2]);
    if(!std::isfinite(warmupMs)||warmupMs<0||warmupMs>60000)throw std::runtime_error("Invalid warmup duration");
    const int warmup=int(warmupMs*12);
    const bool prime=std::stoi(argv[3])!=0,trim=std::stoi(argv[4])!=0;
    const double tolerance=std::stod(argv[5]),humAmplitude=argc>6?std::stod(argv[6]):.03,whiteAmplitude=argc>7?std::stod(argv[7]):.000025;
    if(!std::isfinite(tolerance)||tolerance<0||tolerance>1200||!std::isfinite(humAmplitude)||humAmplitude<0
        ||!std::isfinite(whiteAmplitude)||whiteAmplitude<0)throw std::runtime_error("Invalid scoring/noise parameters");
    const bool singlePartialRescue=argc>8&&std::stoi(argv[8])!=0;
    std::cout<<"file,truth_pitch,guitar_gain,raw_correct_frames,raw_valid_frames,conditioned_correct_frames,conditioned_valid_frames,mains_only_frames,mains_explained_frames,first_mains_explained_ms,first_any_admitted_ms,first_correct_admitted_ms,prime_sample,early_admissions,reconstruction_error_vs_added_noise_db,reconstruction_error_vs_wanted_db\n";
    std::string path;double truthPitch=0;int inventoryOn=0,inventoryOff=0,cases=0;
    while(fixtures>>std::quoted(path)>>truthPitch>>inventoryOn>>inventoryOff)for(double gain:{.003,.01,.03}){
        // Inventory truth is immutable across gains. Mutating it here would
        // trim only the first gain and silently invalidate onset comparisons.
        int onset=inventoryOn,offset=inventoryOff;
        auto wanted=readWave(path);
        if(!std::isfinite(truthPitch)||truthPitch<0||truthPitch>127||onset<0||offset<=onset
            ||offset/4>int(wanted.size()))throw std::runtime_error("Invalid fixture truth interval");
        if(trim){wanted.erase(wanted.begin(),wanted.begin()+onset/4);offset-=onset;onset=0;}
        const int on=onset/4,off=offset/4;
        flysnapper::pitch::NoiseConditioner conditioner(12000);
        flysnapper::pitch::YinEstimator rawEstimator,cleanEstimator;rawEstimator.prepare(12000);cleanEstimator.prepare(12000);
        std::array<float,512> rawRing{},cleanRing{},rawFrame{},cleanFrame{};
        Random random;int write=0,filled=0,hold=0,rawCorrect=0,rawValid=0,cleanCorrect=0,cleanValid=0;
        int firstAny=-1,firstCorrect=-1,primed=-1,early=0,mainsOnly=0,mainsExplained=0,firstExplained=-1;
        double noiseBefore=0,errorAfter=0,wantedPower=0;
        const double truthHz=440*std::pow(2.,(truthPitch-69)/12.);
        const auto correct=[&](const auto& candidate){return candidate.valid&&(tolerance>0?
            std::abs(1200*std::log2(candidate.frequency/truthHz))<tolerance:candidate.note==int(std::round(truthPitch)));};
        for(int i=0;i<warmup+int(wanted.size());++i){
            const double t=i/12000.;
            const double mains=humAmplitude*(std::sin(2*pi*50.03*t)+.4*std::sin(4*pi*50.03*t+.3)+.2*std::sin(6*pi*50.03*t+.7));
            const double tone=i>=warmup?wanted[size_t(i-warmup)]*gain:0,noise=mains+random.next()*whiteAmplitude;
            const float input=float(tone+noise),output=conditioner.processSample(input,hold>0);
            if(hold>0)--hold;
            rawRing[size_t(write)]=input;cleanRing[size_t(write)]=output;write=(write+1)%512;filled=std::min(filled+1,512);
            if(i>=warmup+on&&i<warmup+off){noiseBefore+=noise*noise;errorAfter+=(output-tone)*(output-tone);wantedPower+=tone*tone;}
            if(i%32||filled<128)continue;
            for(int k=0;k<filled;++k){rawFrame[size_t(k)]=rawRing[size_t((write-filled+k+512)%512)];cleanFrame[size_t(k)]=cleanRing[size_t((write-filled+k+512)%512)];}
            const auto raw=rawEstimator.estimate(rawFrame.data(),filled,.08f),clean=cleanEstimator.estimate(cleanFrame.data(),filled,.08f);
            const auto evidence=conditioner.assess(cleanFrame.data(),filled,clean.frequency,clean.confidence,hold>0);
            if(clean.valid&&evidence.mainsExplained&&firstExplained<0&&i>=warmup+on)firstExplained=i-warmup-on;
            // This is the old component-probe policy, not current product admission.
            // Ground truth below is used ONLY for scoring, never for protection.
            if(prime&&clean.valid&&conditioner.primeMains(cleanFrame.data(),filled,clean.frequency,evidence)){
                primed=i;filled=write=0;continue;
            }
            const bool harmonic=evidence.phaseCoherence>.85&&evidence.harmonicFraction>.6&&evidence.coherentPartials>=2;
            const bool periodic=singlePartialRescue&&clean.confidence>.985f&&evidence.harmonicFraction>.4&&evidence.phaseCoherence>.95;
            const bool admitted=clean.valid&&!evidence.mainsOnly&&evidence.snrDb>6&&(harmonic||periodic);
            if(admitted){hold=600;if(i<warmup+on)++early;
                if(i>=warmup+on&&firstAny<0)firstAny=i-warmup-on;
                if(i>=warmup+on&&correct(clean)&&firstCorrect<0)firstCorrect=i-warmup-on;
            }
            if(i>=warmup+on+1200&&i<warmup+off-600){
                rawValid+=raw.valid;rawCorrect+=correct(raw);cleanValid+=clean.valid;cleanCorrect+=correct(clean);
                mainsOnly+=clean.valid&&evidence.mainsOnly;
                mainsExplained+=clean.valid&&evidence.mainsExplained;
            }
        }
        const auto ms=[](int sample){return sample<0?-1.:sample/12.;};
        std::cout<<path<<','<<truthPitch<<','<<gain<<','<<rawCorrect<<','<<rawValid<<','<<cleanCorrect<<','<<cleanValid<<','<<mainsOnly
            <<','<<mainsExplained<<','<<ms(firstExplained)<<','<<ms(firstAny)<<','<<ms(firstCorrect)<<','<<primed<<','<<early<<',';
        if(noiseBefore>0)std::cout<<10*std::log10(errorAfter/noiseBefore);
        std::cout<<',';if(wantedPower>0)std::cout<<10*std::log10(errorAfter/wantedPower);
        std::cout<<'\n';++cases;
    }
    if(!fixtures.eof()||cases==0)throw std::runtime_error("Empty or invalid fixture inventory");
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
