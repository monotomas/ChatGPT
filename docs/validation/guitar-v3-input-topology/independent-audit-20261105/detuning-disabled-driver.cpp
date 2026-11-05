#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
using namespace flysnapper::pitch;
constexpr double pi=3.14159265358979323846;
int main(){
 std::cout<<"rate,anchor,cents,bright,ons,offs,wrong_ons,first_sample,active_end\n";
 for(int rate:{44100,48000,96000})for(int note=28;note<=100;++note)for(int cents:{-40,40})for(bool bright:{false,true}){
  auto d=std::make_unique<PitchDetector>();d->prepare(rate);Settings s;s.inputDynamics.enabled=false;d->configure(s);
  std::vector<float>w(size_t(rate/2),0);const double f=440*std::pow(2.,(note-69+cents/100.)/12.);
  for(int n=0;n<rate/3;++n)for(int h=1;h<=(bright?12:1);++h)if(f*h<rate*.45)
   w[size_t(n)]+=float((bright?.03:.15)*std::sin(2*pi*f*h*n/rate+.17*h)/std::pow(double(h),.8));
  Output out;int ons=0,offs=0,wrong=0,first=-1;
  for(int n=0;n<int(w.size());n+=64){out.clear();d->process(w.data()+n,std::min(64,int(w.size())-n),out);for(size_t k=0;k<out.size;++k){const auto&e=out.events[k];if(e.pitchChange)continue;if(e.on){++ons;wrong+=e.note!=note;if(first<0)first=n+e.offset;}else++offs;}}
  std::cout<<rate<<','<<note<<','<<cents<<','<<bright<<','<<ons<<','<<offs<<','<<wrong<<','<<first<<','<<d->status().activeNotes<<'\n';
 }
}
