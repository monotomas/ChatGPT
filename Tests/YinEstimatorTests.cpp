// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Pitch/YinEstimator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace {
using flysnapper::pitch::YinEstimator;
constexpr double pi=3.14159265358979323846;
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
std::array<float,2048> filteredTone(double pitch,double phase,bool harmonic,bool evenDominant=false){
    const double frequency=440*std::pow(2.,(pitch-69)/12.);
    const double dc=std::exp(-2*pi*12/48000),low=1-std::exp(-2*pi*5040/48000);
    double previous=0,previousDC=0;std::array<double,4> poles{};
    std::array<double,12> cosine{},sine{},stepC{},stepS{},weight{};
    for(int h=1;h<=12;++h){
        const double angle=2*pi*frequency*h/48000;
        stepC[size_t(h-1)]=std::cos(angle);stepS[size_t(h-1)]=std::sin(angle);
        cosine[size_t(h-1)]=std::cos(phase*h+.17*h);sine[size_t(h-1)]=std::sin(phase*h+.17*h);
        if(frequency*h<21600&&(harmonic||h==1))
            weight[size_t(h-1)]=evenDominant?(h%2?.30/std::sqrt(double(h)):1/std::pow(double(h/2),1.3)):1/std::pow(double(h),1.25);
    }
    std::array<float,2048> audio{};
    for(int i=0;i<8192;++i){
        double input=0;
        for(int h=0;h<12;++h){
            input+=weight[size_t(h)]*sine[size_t(h)];
            const double next=cosine[size_t(h)]*stepC[size_t(h)]-sine[size_t(h)]*stepS[size_t(h)];
            sine[size_t(h)]=sine[size_t(h)]*stepC[size_t(h)]+cosine[size_t(h)]*stepS[size_t(h)];cosine[size_t(h)]=next;
        }
        input*=.15*std::min(1.,i/48.);
        double value=input-previous+dc*previousDC;previous=input;previousDC=value;
        for(auto& pole:poles){pole+=low*(value-pole);value=pole;}
        if(i%4==3)audio[size_t(i/4)]=float(value);
    }
    return audio;
}
void roundedRangeAndCoherentSeries(){
    YinEstimator estimator;estimator.prepare(12000);
    for(int note=28;note<=100;++note)for(double phase:{0.,.31,1.2,2.6,5.1})
        for(double cents:{-40.,0.,40.})for(bool harmonic:{false,true}){
            const auto audio=filteredTone(note+cents/100,phase,harmonic);
            const int count=note<36?1024:512;
            const auto result=estimator.estimate(audio.data()+audio.size()-count,count);
            check(result.valid&&result.note==note,"YIN rounded-range frontend control failed");
        }
    for(double pitch:{27.4,100.6})for(double phase:{0.,1.2}){
        const auto audio=filteredTone(pitch,phase,false);
        check(!estimator.estimate(audio.data()+1024,1024).valid,"YIN output escaped configured MIDI range");
    }
    // These fundamentals retain physically resolved odd partials even though
    // most energy belongs to the octave series. Preserve that lower identity.
    for(int note:{40,52,64,72,84})for(double phase:{0.,.731,1.462})for(double cents:{-25.,17.}){
        const auto audio=filteredTone(note+cents/100,phase,true,true);
        const auto result=estimator.estimate(audio.data()+1536,512);
        check(result.valid&&result.note==note,"YIN coherent weak odd series was promoted an octave");
    }
    std::array<float,1024> body{};
    for(int i=0;i<1024;++i)body[size_t(i)]=float(.1*std::sin(2*pi*110*i/12000.)+.3*std::sin(2*pi*220*i/12000.)+.18*std::sin(2*pi*440*i/12000.)+.09*std::sin(2*pi*660*i/12000.));
    const auto result=estimator.estimate(body.data(),1024);
    check(result.valid&&result.note==57&&result.harmonicDivisor==2,"YIN useful even-series body disambiguation was lost");
}
void shortFrameIsolation(){
    // Results may depend only on the supplied frame, never on old scratch
    // samples beyond its end. The shortest cubic refinements exercise that
    // contract with different preceding buffers and exact-sized allocations.
    YinEstimator first,second;first.prepare(12000);second.prepare(12000);
    const auto low=filteredTone(40,0.,true),high=filteredTone(84,1.2,true);
    first.estimate(low.data(),1024);second.estimate(high.data(),1024);
    for(int count:{32,33,34,35,40,48,63,64})for(int note=75;note<=100;++note)for(double phase:{0.,.31,1.2,2.6,5.1}){
        std::vector<float> frame(size_t(count),0.f);
        const double frequency=440*std::pow(2.,double(note-69)/12.);
        for(int i=0;i<count;++i)for(int h=1;h<=8;++h)if(frequency*h<5400)
            frame[size_t(i)]+=float(.15/h*std::sin(2*pi*frequency*h*i/12000.+phase*h));
        const auto a=first.estimate(frame.data(),count),b=second.estimate(frame.data(),count);
        check(a.valid==b.valid&&a.note==b.note&&a.frequency==b.frequency&&a.pitch==b.pitch
            &&a.confidence==b.confidence&&a.periodicity==b.periodicity&&a.harmonicDivisor==b.harmonicDivisor,
            "YIN short frame depended on previous scratch samples");
    }
}
void synthetic(){
    YinEstimator estimator;estimator.prepare(12000);
    std::array<float,1024> x{};
    for(int note=28;note<=100;++note){
        const double hz=440*std::pow(2.,double(note-69)/12.);
        for(int i=0;i<1024;++i)x[size_t(i)]=float(.2*std::sin(2*pi*hz*i/12000.+.37));
        const auto result=estimator.estimate(x.data(),1024);
        if(!result.valid||result.note!=note)std::cerr<<"Puretone reference="<<note<<" valid="<<result.valid<<" got="<<result.note<<" pitch="<<result.pitch<<" cmnd_conf="<<result.periodicity<<'\n';
        check(result.valid&&result.note==note,"YIN puretone note mismatch");
        check(result.confidence>.90f,"YIN puretone confidence low");
    }
    for(int note:{40,45,52,64,76}){
        const double hz=440*std::pow(2.,double(note-69)/12.);
        x.fill(0);
        for(int h=1;h<=12;++h)if(h*hz<5400)for(int i=0;i<1024;++i)x[size_t(i)]+=float(.12/h*std::sin(2*pi*hz*h*i/12000.+.21*h));
        const auto result=estimator.estimate(x.data(),512);
        check(result.valid&&result.note==note,"YIN harmonic note mismatch");
    }
    // Body-resonance outlier plus a dominant165Hz series has common55Hz
    // periodicity. The harmonic-series check should preserve intended165Hz.
    for(int i=0;i<1024;++i)x[size_t(i)]=float(.12*std::sin(2*pi*110*i/12000.)+.3*std::sin(2*pi*165*i/12000.)+.18*std::sin(2*pi*330*i/12000.)+.09*std::sin(2*pi*495*i/12000.));
    const auto raw=estimator.estimate(x.data(),1024,.08f,false),validated=estimator.estimate(x.data(),1024);
    check(raw.valid&&raw.note==33,"YIN common-divisor control changed");
    if(!validated.valid||validated.note!=52||validated.harmonicDivisor!=3)std::cerr<<"Body raw="<<raw.note<<" adjusted="<<validated.note<<" div="<<validated.harmonicDivisor<<" pitch="<<validated.pitch<<" purity="<<validated.harmonicPurity<<'\n';
    check(validated.valid&&validated.note==52&&validated.harmonicDivisor==3,"YIN body-resonance series disambiguation failed");
    for(int i=0;i<1024;++i)x[size_t(i)]=float(.2*std::sin(2*pi*440*std::pow(2.,(52.4-69)/12.)*i/12000.));
    const auto detuned=estimator.estimate(x.data(),512);check(detuned.valid&&detuned.note==52,"YIN stable40cent detuning was muted");
    x.fill(0);check(!estimator.estimate(x.data(),512).valid,"YIN silence accepted");
    x.fill(.2f);check(!estimator.estimate(x.data(),512).valid,"YIN DC accepted");
    x.fill(std::numeric_limits<float>::quiet_NaN());check(!estimator.estimate(x.data(),512).valid,"YIN nonfinite input accepted");
    check(!estimator.estimate(nullptr,512).valid,"YIN null input accepted");
    check(!estimator.estimate(x.data(),16).valid,"YIN short input accepted");
    uint32_t seed=56789;
    for(int trial=0;trial<100;++trial){
        for(auto& value:x){seed=1664525u*seed+1013904223u;value=float((double(seed)/4294967296.-.5)*.2);}
        check(!estimator.estimate(x.data(),512).valid,"YIN white noise accepted");
    }
    // The real frontend attenuates upper harmonics and can make an integer
    // double period look better than a fractional first period. Check the
    // actual causal48kHz->12kHz path, not just long unfiltered sine frames.
    for(int note=79;note<=100;++note)for(double phase:{0.,.31,1.2,2.6,5.1}){
        const double frequency=440*std::pow(2.,double(note-69)/12.);
        const double dc=std::exp(-2*pi*12/48000),low=1-std::exp(-2*pi*5040/48000);
        double previousX=0,previousDC=0;std::array<double,4> poles{};std::array<float,1024> filtered{};
        for(int i=0;i<4096;++i){
            double input=0,t=double(i)/48000.;
            for(int h=1;h<=12;++h)if(frequency*h<21600)
                input+=std::sin(2*pi*frequency*h*t+.17*h+phase*h)/std::pow(double(h),1.25);
            input*=.15*std::min(1.,t/.001);
            double value=input-previousX+dc*previousDC;previousX=input;previousDC=value;
            for(auto& pole:poles){pole+=low*(value-pole);value=pole;}
            if(i%4==3)filtered[size_t(i/4)]=float(value);
        }
        for(int end:{128,192,256,384,512,768,1024}){
            const int count=std::min(end,512);
            const auto result=estimator.estimate(filtered.data()+end-count,count);
            if(!result.valid||result.note!=note)std::cerr<<"Filtered reference="<<note<<" phase="<<phase<<" end="<<end<<" valid="<<result.valid<<" got="<<result.note<<" pitch="<<result.pitch<<'\n';
            check(result.valid&&result.note==note,"YIN filtered short-frame high note octave error");
        }
    }
    bool rejected=false;try{estimator.prepare(0);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"YIN invalid preparation accepted");
}
std::vector<float> readWave(const std::filesystem::path& file){
    std::ifstream in(file,std::ios::binary);if(!in)throw std::runtime_error("Cannot read YIN guitar fixture");
    const std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)),{});
    const auto u16=[&](size_t i){if(i+2>b.size())throw std::runtime_error("Truncated WAV");return uint16_t(b[i])|(uint16_t(b[i+1])<<8);};
    const auto u32=[&](size_t i){return uint32_t(u16(i))|(uint32_t(u16(i+2))<<16);};
    if(b.size()<12||std::string(reinterpret_cast<const char*>(b.data()),4)!="RIFF")throw std::runtime_error("Not WAV");
    size_t data=0,length=0;int rate=0,channels=0,bits=0,format=0;
    for(size_t at=12;at+8<=b.size();){const auto count=u32(at+4);if(at+8+uint64_t(count)>b.size())throw std::runtime_error("WAV chunk outside file");
        const std::string name(reinterpret_cast<const char*>(b.data()+at),4);
        if(name=="fmt "){format=u16(at+8);channels=u16(at+10);rate=int(u32(at+12));bits=u16(at+22);}
        else if(name=="data"){data=at+8;length=count;}
        at+=8+count+(count&1);
    }
    if(format!=1||channels!=1||bits!=16||rate!=48000||!data)throw std::runtime_error("YIN fixture must be48kHz mono PCM16");
    std::vector<float> out(length/2);for(size_t i=0;i<out.size();++i)out[i]=float(int16_t(u16(data+2*i)))/32768.f;
    return out;
}
void recordings(const std::filesystem::path& folder){
    for(const auto& item:std::array<std::pair<const char*,int>,8>{{{"E2",40},{"A2",45},{"D3",50},{"G3",55},{"B3",59},{"E3",52},{"E4",64},{"Fs3",54}}}){
        const auto audio=readWave(folder/"pcm"/(std::string("recorded_")+item.first+".wav"));
        int onset=-1;for(size_t i=0;i<audio.size();++i)if(std::abs(audio[i])>=.001f){onset=int(i);break;}
        check(onset>=0,"Guitar onset absent");
        std::vector<float> decimated;decimated.reserve(audio.size()/4);
        const double dc=std::exp(-2*pi*12/48000),low=1-std::exp(-2*pi*(12000*.42)/48000);
        double previousX=0,previousDC=0;std::array<double,4> stage{};
        for(size_t i=0;i<audio.size();++i){const double filtered=audio[i]-previousX+dc*previousDC;previousX=audio[i];previousDC=filtered;
            double x=filtered;for(auto& pole:stage){pole+=low*(x-pole);x=pole;}
            if(i%4==3)decimated.push_back(float(x));
        }
        YinEstimator estimator;estimator.prepare(12000);
        int previous=-1,repeated=0,first=-1,firstNote=-1,wrong=0;
        for(int end=64;end<=int(decimated.size());end+=32){
            const int count=std::min(512,end);
            const auto result=estimator.estimate(decimated.data()+end-count,count);
            const int note=result.valid?result.note:-1;
            if(previous==note)++repeated;else{previous=note;repeated=1;}
            if(repeated==3&&note>=0){if(first<0){first=end*4-1;firstNote=note;}if(note!=item.second)++wrong;}
        }
        check(firstNote==item.second,"YIN recorded guitar first note wrong or absent");
        check(wrong==0,"YIN recorded guitar wrong confirmed event");
        const double latency=1000.*(first-onset)/48000.;
        check(latency>=0&&latency<100,"YIN recorded guitar acquisition exceeded100ms control");
        std::cout<<"YIN recorded "<<item.first<<": first="<<firstNote<<" observed_ms="<<latency<<" wrong_confirmations="<<wrong<<'\n';
    }
}
int run(const std::vector<std::filesystem::path>& args){try{
    synthetic();roundedRangeAndCoherentSeries();shortFrameIsolation();if(args.size()==2)recordings(args[1]);
    else if(args.size()>2)throw std::runtime_error("YinEstimatorTests [AudioFixtures directory]");
    std::cout<<"YinEstimator checks passed: "<<checks<<". Candidate/confirmation controls, not complete audio lifecycle or physical latency.\n";return 0;
}catch(const std::exception& e){std::cerr<<"YinEstimator test failed: "<<e.what()<<'\n';return 1;}}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::filesystem::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return run(args);}
#else
int main(int argc,char** argv){std::vector<std::filesystem::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return run(args);}
#endif
