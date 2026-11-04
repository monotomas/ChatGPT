// SPDX-License-Identifier: AGPL-3.0-or-later
// Causal positive controls and an added-quiet-source falsification. All signal
// source intervals/pitches are fixed before either executable is measured.
#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
constexpr double pi=3.14159265358979323846;
struct Case {std::string name;double firstHz;bool bend=false;double secondLevel=0,secondHz=73.4161919794,bendTarget=50;};
double midi(double hz){return 69+12*std::log2(hz/440.);}
int main(int argc,char**argv){
 if(argc<2||argc>3)return 2;const bool full=argc==3;
 std::string root=argv[1];std::ofstream metrics(root+"-metrics.csv"),raw(root+"-events.csv");
 if(!metrics||!raw)return 3;
 metrics<<"case,poly,rate,block,on_count,off_count,hung,overflow,max_us,deadline_misses,cpu_p99_us,cpu_max_us\n";
 raw<<"case,poly,rate,block,sample,type,note,pitch,voice\n";raw<<std::setprecision(10);
 std::vector<Case> cases={{"G1_49",48.9994295},{"Bb1_58",58.2704702},{"B1_61",61.7354127},{"G2_98",97.998859},{"F4_near350",349.228231},{"E5_near660",659.255114},{"F5_near700",698.456463},{"G1_bend_to50",48.9994295,true}};
 for(double level:{.012,.016,.020,.025,.040})cases.push_back({"G1_bend50_then_D2_"+std::to_string(level),48.9994295,true,level});
 for(double level:{.08,.12,.20})cases.push_back({"G1_bend50_then_D2_"+std::to_string(level),48.9994295,true,level});
 cases.push_back({"F4_near350_then_A4",349.228231,false,.060,440.});
 cases.push_back({"F4_bend350_then_A4",349.228231,true,.060,440.,350.});
 cases.push_back({"E5_near660_then_A4",659.255114,false,.060,440.});
 for(const auto& c:cases)for(bool poly:{false,true})for(int rate:full?std::vector<int>{44100,48000,96000}:std::vector<int>{48000})for(int block:full?std::vector<int>{32,64,128}:std::vector<int>{64}){
  auto detector=std::make_unique<flysnapper::pitch::PitchDetector>();detector->prepare(rate);flysnapper::pitch::Settings settings;
  settings.autoInput=true;settings.polyphonic=poly;settings.monoBend=true;settings.polyBend=poly;settings.bendRangeSemitones=48;detector->configure(settings);
  const int length=int(rate*1.2);std::vector<float> audio(size_t(length),0);double phase=0,second=0;
  for(int n=0;n<length;++n){const double t=double(n)/rate;
   const double f=c.firstHz+(c.bend?(c.bendTarget-c.firstHz)*std::clamp((t-.25)/.20,0.,1.):0.);
   phase+=2*pi*f/rate;second+=2*pi*c.secondHz/rate;
   const double end=c.secondLevel>0?.95:.85;
   const auto shape=[](double p){return std::sin(p)+.30*std::sin(2*p+.2)+.11*std::sin(3*p+.6);};
   if(t>=.05&&t<end)audio[size_t(n)]=float(.14*std::min(1.,(t-.05)/.002)*std::exp(-.3*(t-.05))*shape(phase));
   if(c.secondLevel>0&&t>=.60&&t<.95)audio[size_t(n)]+=float(c.secondLevel*std::min(1.,(t-.60)/.002)*shape(second));
  }
  flysnapper::pitch::Output output;int ons=0,offs=0,overflow=0,misses=0;double maximum=0;std::vector<double> cpuTimes;
  for(int at=0;at<length;at+=block){const int count=std::min(block,length-at);output.clear();const auto before=std::chrono::steady_clock::now();const auto cpuBefore=std::clock();detector->process(audio.data()+at,count,output);const auto cpuAfter=std::clock();const auto after=std::chrono::steady_clock::now();cpuTimes.push_back(1e6*(cpuAfter-cpuBefore)/CLOCKS_PER_SEC);
   const double us=std::chrono::duration<double,std::micro>(after-before).count();maximum=std::max(maximum,us);misses+=us>1e6*count/rate;overflow+=output.overflow;
   for(size_t j=0;j<output.size;++j){const auto&e=output.events[j];
    if(!e.pitchChange){if(e.on)++ons;else++offs;}
    raw<<c.name<<','<<poly<<','<<rate<<','<<block<<','<<at+e.offset<<','<<(e.pitchChange?'P':e.on?'N':'F')<<','<<e.note<<','<<e.pitch<<','<<e.voiceId<<'\n';
   }
  }
  std::sort(cpuTimes.begin(),cpuTimes.end());
  metrics<<c.name<<','<<poly<<','<<rate<<','<<block<<','<<ons<<','<<offs<<','<<detector->status().activeNotes<<','<<overflow<<','<<maximum<<','<<misses<<','<<cpuTimes[size_t(std::ceil(.99*(cpuTimes.size()-1)))]<<','<<cpuTimes.back()<<'\n';
 }
 std::cout<<"Positive-control raw capture completed; not an acceptance result.\n";
}
