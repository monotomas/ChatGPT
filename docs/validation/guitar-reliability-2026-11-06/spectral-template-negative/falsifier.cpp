// SPDX-License-Identifier: AGPL-3.0-or-later
// Private causal spectral-template falsifier. Not a product replacement.
#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
int main(){
    constexpr double pi=3.14159265358979323846;constexpr int sr=48000,block=64;
    const std::array<std::string,6> names{{"single_normal","single_even","single_even_to_odd_morph","single_12_partials","late_added_octave","late_added_near_third"}};
    auto detector=std::make_unique<flysnapper::pitch::PitchDetector>();
    std::cout<<"case,dynamics,phase_variant,sample,type,note\n";
    std::cerr<<"case,dynamics,phase_variant,callbacks,callback_p99_us,callback_max_us,deadline_misses\n";
    for(int which=0;which<int(names.size());++which)for(int dyn:{0,1})for(int phaseVariant:{0,1}){
        detector->prepare(sr);flysnapper::pitch::Settings settings;settings.polyphonic=true;settings.inputDynamics.enabled=dyn;detector->configure(settings);
        std::vector<float> input(sr);std::vector<double> times;int deadlineMisses=0;
        for(int n=0;n<sr;++n){
            const double time=double(n)/sr;double value=0;const int sources=which>=4?2:1;
            for(int j=0;j<sources;++j){
                const double onset=j?.28:.08,local=time-onset;if(local<0||local>=.60)continue;
                const int note=which==5?(j?64:45):(j?52:40);const double frequency=440*std::exp2((note-69)/12.);
                const double amplitude=.08/sources*std::min(local/.002,1.)*std::exp(-local/1.1);
                for(int h=1;h<=(which==3?12:8);++h){
                    double parity=1.;
                    if(which==1)parity=h%2?.15:1.;
                    if(which==2){const double blend=std::clamp((local-.18)/.20,0.,1.);parity=h%2?.15+.85*blend:1.-.85*blend;}
                    const double phase=h*.391+j*.76+(j||sources==1?phaseVariant*pi:0.);
                    value+=amplitude*parity/std::pow(double(h),1.2)*std::sin(2*pi*frequency*h*local+phase);
                }
            }
            input[size_t(n)]=float(value);
        }
        for(int at=0;at<sr;at+=block){
            flysnapper::pitch::Output output;const auto begin=std::chrono::steady_clock::now();
            detector->process(input.data()+at,std::min(block,sr-at),output);
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
            times.push_back(us);deadlineMisses+=us>1e6*std::min(block,sr-at)/sr;
            for(size_t k=0;k<output.size;++k){const auto& e=output.events[k];if(!e.pitchChange)std::cout<<names[size_t(which)]<<','<<dyn<<','<<phaseVariant<<','<<at+e.offset<<','<<(e.on?'N':'F')<<','<<e.note<<'\n';}
        }
        std::sort(times.begin(),times.end());
        std::cerr<<names[size_t(which)]<<','<<dyn<<','<<phaseVariant<<','<<times.size()<<','<<std::setprecision(9)<<times[size_t(std::ceil(.99*(times.size()-1)))]<<','<<times.back()<<','<<deadlineMisses<<'\n';
    }
}
