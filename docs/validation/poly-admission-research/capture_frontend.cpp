// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#define private public
#include "Pitch/PitchDetector.h"
#undef private
int main(int argc,char**argv){
 if(argc!=3)return 2;
 const bool automatic=std::stoi(argv[1])!=0;
 auto detector=std::make_unique<flysnapper::pitch::PitchDetector>();detector->prepare(48000);
 flysnapper::pitch::Settings settings;settings.polyphonic=true;settings.polyBend=true;settings.autoInput=automatic;settings.bendRangeSemitones=48;detector->configure(settings);
 std::ofstream out(argv[2]);out<<"analysis_sample,history_index,value\n";
 constexpr double pi=3.14159265358979323846;double changing=0,fixed=0;
 std::array<float,128> samples{};flysnapper::pitch::Output events;
 for(int start=0;start<48000*.4;start+=128){
  for(int n=0;n<128;++n){const double t=(start+n)/48000.;
   changing+=2*pi*110/48000.;fixed+=2*pi*440*std::pow(2.,(64.-69)/12.)/48000.;
   const auto string=[](double p){return std::sin(p)+.20*std::sin(2*p+.2)+.08*std::sin(3*p+.4);};
   samples[size_t(n)]=float(std::min(1.,t/.002)*std::exp(-.18*t)*(.15*string(changing)+.11*string(fixed)));
  }
  events.clear();detector->process(samples.data(),128,events);
  if(detector->filled>=512&&detector->analysisClock%256==0){
   for(int n=0;n<512;++n)out<<detector->analysisClock<<','<<n<<','<<detector->ring[size_t((detector->writeIndex-512+n+detector->windowSize)%detector->windowSize)]<<'\n';
  }
  for(size_t j=0;j<events.size;++j)if(events.events[j].on)std::cout<<"On "<<events.events[j].note<<" @"<<start+events.events[j].offset<<"\n";
 }
}
