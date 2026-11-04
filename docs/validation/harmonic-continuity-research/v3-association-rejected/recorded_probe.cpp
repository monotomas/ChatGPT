// SPDX-License-Identifier: AGPL-3.0-or-later
// Oracle known-pitch observer mechanism probe; not MIDI/acquisition proof.
#include "Pitch/HarmonicContinuity.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace {
unsigned u16(const std::vector<unsigned char>& b,size_t p){return b.at(p)|(unsigned(b.at(p+1))<<8);}
unsigned u32(const std::vector<unsigned char>& b,size_t p){return u16(b,p)|(u16(b,p+2)<<16);}
struct Wave {unsigned rate=0;std::vector<float> samples;};
Wave read(const char* path){
    std::ifstream stream(path,std::ios::binary);if(!stream)throw std::runtime_error("Cannot open PCM input");
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)),{});
    if(bytes.size()<12||std::memcmp(bytes.data(),"RIFF",4)||std::memcmp(bytes.data()+8,"WAVE",4))throw std::runtime_error("Not RIFF PCM");
    unsigned channels=0,format=0,bits=0;size_t start=0,length=0;Wave out;
    for(size_t at=12;at+8<=bytes.size();){const auto size=u32(bytes,at+4);if(at+8+size>bytes.size())throw std::runtime_error("Truncated PCM");
        if(!std::memcmp(bytes.data()+at,"fmt ",4)){format=u16(bytes,at+8);channels=u16(bytes,at+10);out.rate=u32(bytes,at+12);bits=u16(bytes,at+22);}
        if(!std::memcmp(bytes.data()+at,"data",4)){start=at+8;length=size;}
        at+=8+size+(size&1);
    }
    if(format!=1||bits!=16||channels<1||out.rate<12000||!start)throw std::runtime_error("Expected PCM16");
    for(size_t at=start;at+channels*2<=start+length;at+=channels*2){double x=0;for(unsigned ch=0;ch<channels;++ch)x+=std::int16_t(u16(bytes,at+2*ch))/32768.;out.samples.push_back(float(x/channels));}
    return out;
}
}
int main(int argc,char** argv){try{
    if(argc!=6)throw std::runtime_error("recorded_probe PCM NOTE TRUE_ON_SAMPLES48K TRUE_OFF_SAMPLES48K CSV");
    const auto wave=read(argv[1]);const double rate=wave.rate,analysisRate=12000,pi=3.14159265358979323846;
    const int note=std::stoi(argv[2]);const double begin=std::stod(argv[3])/48000.,end=std::stod(argv[4])/48000.;
    const float frequency=float(440*std::pow(2.,(note-69)/12.));
    flysnapper::pitch::HarmonicContinuity observer;observer.prepare(analysisRate);
    std::array<float,1024> ring{},frame{};int written=0,filled=0,hop=0;std::int64_t tick=0;
    const double step=analysisRate/rate,lowpass=1-std::exp(-2*pi*analysisRate*.42/rate),dcPole=std::exp(-2*pi*12/rate);
    double phase=0,dcX=0,dcY=0,l1=0,l2=0,l3=0,l4=0,previous=0;
    std::ofstream output(argv[5]);if(!output)throw std::runtime_error("Cannot write CSV");
    output<<"sample,time,within_reference,phase_support,continuing,changed,departure,continuity,foreground_novelty,residual_novelty,novelty_fraction,energy_support,reset,reset_coherence,foreground,residual";
    for(int h=1;h<=8;++h)output<<",h"<<h<<"_phase,h"<<h<<"_departure,h"<<h<<"_power,h"<<h<<"_baseline,h"<<h<<"_recent,h"<<h<<"_snr,h"<<h<<"_trusted,h"<<h<<"_continuing";
    output<<'\n';
    for(size_t n=0;n<wave.samples.size()+size_t(rate*.25);++n){
        const double x=n<wave.samples.size()?wave.samples[n]:0,d=x-dcX+dcPole*dcY;dcX=x;dcY=d;
        l1+=lowpass*(d-l1);l2+=lowpass*(l1-l2);l3+=lowpass*(l2-l3);l4+=lowpass*(l3-l4);
        const double prior=previous;previous=l4;phase+=step;if(phase<1)continue;phase-=1;
        const float value=float(prior+(1-phase/step)*(l4-prior));
        ring[size_t(written)]=value;written=(written+1)%1024;filled=std::min(filled+1,1024);++tick;
        if(++hop<32)continue;
        hop=0;
        const double time=n/rate;if(time<begin+.060)continue;
        for(int k=0;k<filled;++k)frame[size_t(k)]=ring[size_t((written-filled+k+1024)%1024)];
        const auto e=observer.observe(frame.data(),filled,frequency,tick-1);
        output<<n<<','<<time<<','<<(time>=begin&&time<end)<<','<<e.phaseSupport<<','<<e.continuingPartials<<','<<e.changedPartials<<','<<e.departurePartials<<','<<e.phaseContinuity<<','<<e.foregroundNovelty<<','<<e.residualNovelty<<','<<e.noveltyFraction<<','<<e.coherentEnergyRisePartials<<','<<e.commonResetRadians<<','<<e.resetCoherence<<','<<e.foregroundPower<<','<<e.residualPower;
        for(const auto& p:e.partials)output<<','<<p.phaseInnovation<<','<<p.phaseDeparture<<','<<p.power<<','<<p.baselinePower<<','<<p.recentPower<<','<<p.projectionSnrDb<<','<<p.phaseTrusted<<','<<p.continuing;
        output<<'\n';
    }
    if(!output)throw std::runtime_error("CSV write failed");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
