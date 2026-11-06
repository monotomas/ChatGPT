// Corrected v2: all components of a re-excited string damp together.
// Original v1 and its independent first-component cutoff remain retained.
#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
int main(){
 constexpr double pi=3.14159265358979323846; constexpr int sr=48000,block=64,frames=60000;
 auto detector=std::make_unique<flysnapper::pitch::PitchDetector>();
 std::cout<<"wave,phase,new_gain,dynamics,sample,type,note\n";
 for(int kind=0;kind<4;++kind)for(double phase:{0.,.9,2.1})for(double newGain:{.5,1.})for(int dynamics:{0,1}){
  std::vector<int> notes=kind==2?std::vector<int>{45,52,57,60,64}:std::vector<int>{40,45,50,55,59,64};
  detector->prepare(sr);flysnapper::pitch::Settings settings;settings.polyphonic=true;settings.inputDynamics.enabled=dynamics;detector->configure(settings);
  std::vector<float> input(frames);
  for(int n=0;n<frames;++n){const double t=double(n)/sr;double value=0;
   for(int gesture=0;gesture<((kind==1||kind==2)?2:1);++gesture)for(size_t j=0;j<notes.size();++j){
    const double start=.08+.32*gesture+j*.007,a=t-start;if(a<0||t>(kind==1||kind==2?1.0+j*.007:.68+j*.007))continue;
    const double frequency=440*std::exp2(double(notes[j]-69)/12.);
    const double amplitude=.08/notes.size()*(gesture?newGain:1.)*std::min(a/.002,1.)*std::exp(-a/1.1);
    for(int h=1;h<=8;++h)value+=amplitude/std::pow(double(h),1.2)*std::sin(2*pi*frequency*h*a+h*.391+j*.76+(gesture?phase+h*.17+j*.31:0.));
   }
   if(kind==3)value*=1+.8*std::clamp((t-.36)/.08,0.,1.);
   input[size_t(n)]=float(value);
  }
  for(int at=0;at<frames;at+=block){flysnapper::pitch::Output out;detector->process(input.data()+at,std::min(block,frames-at),out);for(size_t k=0;k<out.size;++k){const auto &event=out.events[k];if(!event.pitchChange)std::cout<<kind<<','<<phase<<','<<newGain<<','<<dynamics<<','<<at+event.offset<<','<<(event.on?'N':'F')<<','<<event.note<<'\n';}}
 }
}
