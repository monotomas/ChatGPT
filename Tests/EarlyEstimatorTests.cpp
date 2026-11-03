// SPDX-License-Identifier: AGPL-3.0-or-later
// Standalone research test. Not a substitute for PitchDetector/real guitar tests.
#include "Pitch/EarlyEstimator.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#ifdef FLYSNAPPER_TIME_DOMAIN_TESTS
#include "Pitch/TimeDomainPursuit.h"
#endif
using flysnapper::pitch::EarlyEstimator;
namespace {
int checks=0;
void check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
constexpr double pi=3.14159265358979323846;
void sine(std::array<float,512>& out,int note,double amplitude=.3,double phase=.37){
    double f=440*std::pow(2.,double(note-69)/12.);
    for(int i=0;i<512;++i)out[size_t(i)]=float(amplitude*std::sin(2*pi*f*i/48000.+phase));
}
}
int main(){
    try{
        EarlyEstimator estimator;
        std::array<float,512> data{};
        check(!estimator.estimate(data.data(),512).accepted,"unprepared estimator accepted");
        int totalAccepted=0,totalWrong=0,totalUnknown=0;
        for(int length:{192,256,384,512}){
            estimator.prepare(48000,length);
            check(!estimator.estimate(data.data(),length-1).accepted,"insufficient observation accepted");
            check(!estimator.estimate(nullptr,length).accepted,"null input accepted");
            data.fill(0);check(estimator.estimate(data.data(),length).bestNote==-1,"silence accepted");
            data.fill(std::numeric_limits<float>::quiet_NaN());check(estimator.estimate(data.data(),length).bestNote==-1,"NaN accepted");
            data.fill(.1f);check(estimator.estimate(data.data(),length).bestNote==-1,"DC accepted");
            int accepted=0,wrong=0;
            for(int note=40;note<=88;++note){
                sine(data,note);
                // Use exactly the prepared prefix, not a different suffixphase.
                auto result=estimator.estimate(data.data(),length);
                check(std::isfinite(result.confidence)&&result.confidence>=0&&result.confidence<=1,"invalid confidence");
                check(result.fit>=0&&result.fit<=1,"invalid fit");
                if(result.accepted){++accepted;++totalAccepted;if(result.notes[0]!=note){++wrong;++totalWrong;}}
                else ++totalUnknown;
                check(result.count==(result.accepted?1:0),"accept/count disagreement");
                auto poly=estimator.estimate(data.data(),length,6);
                check(!poly.accepted&&poly.count==0,"single harmonic model claimed polyphonic evidence");
            }
            // The conservative heuristic is tested against its actual intended
            // puretone condition; low notes are allowed to abstain, never to
            // count an earlier wrong pitch as lowlatency success.
            check(accepted>0,"did not accept any clean high puretones");
            check(wrong==0,"accepted wrong puretone pitch");
            std::cout<<"length="<<length<<" observation_ms="<<1000.*length/48000<<" puretone_accepted="<<accepted<<" wrong="<<wrong<<" /49\n";
        }
        estimator.prepare(48000,384);
        unsigned seed=123456789;
        int falseNoise=0;
        for(int trial=0;trial<100;++trial){
            for(auto& sample:data){seed=1664525u*seed+1013904223u;sample=float((double(seed)/4294967296.-.5)*.2);}
            if(estimator.estimate(data.data(),384).accepted)++falseNoise;
        }
        check(falseNoise==0,"white noise accepted");
        bool threw=false;try{estimator.prepare(48000,513);}catch(const std::invalid_argument&){threw=true;}
        check(threw,"oversized observation accepted");
#ifdef FLYSNAPPER_TIME_DOMAIN_TESTS
        flysnapper::pitch::TimeDomainPursuit pursuit;
        pursuit.prepare(12000,512);
        std::array<float,73> strengths{},confidences{};
        data.fill(0);pursuit.analyse(data.data(),512,strengths,confidences);
        for(float value:strengths)check(value==0,"pursuit reported silence note");
        const auto render=[&](std::initializer_list<int> notes){
            data.fill(0);
            for(int note:notes)for(int harmonic=1;harmonic<=8;++harmonic)for(int i=0;i<512;++i)
                data[size_t(i)]+=float(.1/harmonic*std::sin(2*pi*440*std::pow(2.,double(note-69)/12.)*harmonic*i/12000.+.33*harmonic));
        };
        const auto expected=[&](std::initializer_list<int> notes){
            pursuit.analyse(data.data(),512,strengths,confidences);
            for(int i=0;i<73;++i){
                bool present=false;for(int note:notes)present|=i+28==note;
                check((strengths[size_t(i)]>0)==present,"pursuit synthetic pitch set mismatch");
                check(std::isfinite(confidences[size_t(i)])&&confidences[size_t(i)]>=0&&confidences[size_t(i)]<=1,"pursuit confidence outside range");
            }
        };
        render({48,52,55});expected({48,52,55});
        render({52,53});expected({52,53});
        render({69});expected({69});
        render({48,52,55});pursuit.analyse(data.data(),512,strengths,confidences,2);
        int voices=0;for(float value:strengths)voices+=value>0;check(voices<=2,"pursuit polyphony limit exceeded");
        data.fill(std::numeric_limits<float>::quiet_NaN());pursuit.analyse(data.data(),512,strengths,confidences);
        for(float value:strengths)check(value==0,"pursuit accepted NaN input");
        std::cout<<"TimeDomainPursuit: silence, NaN, triad, neighbouring semitones, puretone, bounded voices passed.\n";
#endif
        std::cout<<"EarlyEstimator research: "<<checks<<" checks; accepted="<<totalAccepted<<" unknown="<<totalUnknown<<" wrong="<<totalWrong<<". Not a guitar or polyphonic latency claim.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"EarlyEstimator research FAIL: "<<e.what()<<'\n';return 1;}
}
