#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <cstdlib>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
int main(int argc,char**argv){
 constexpr double pi=3.14159265358979323846; constexpr int sr=48000, block=64;
 const std::vector<std::vector<int>> chords{{40,52,64},{41,48,53,56,60,65},{47,54,59,63,66,71},{64,69,74,79,83,88},{45,52,57,60,64},{43,47,50,55,59,67},{50,57,62,66},{40,52},{45,64},{38,45,50,55,59,64},{28,35,40,45}};
 auto det=std::make_unique<flysnapper::pitch::PitchDetector>();
 std::cout<<"case,cents,timbre,dynamics,sample,type,note\n";
 for(size_t c=0;c<chords.size();++c)for(int cents: {-20,0,20})for(int timbre: {0,1})for(int dynamics:{0,1}){
 if(argc>1&&int(c)!=std::atoi(argv[1]))continue;
 det->prepare(sr); flysnapper::pitch::Settings s;s.polyphonic=true;s.inputDynamics.enabled=dynamics;det->configure(s);
 std::vector<float> input(sr); for(int n=0;n<sr;++n){double t=double(n)/sr;double val=0;
 for(size_t j=0;j<chords[c].size();++j){double start=(c==7||c==8)?(j==0?.08:.28):.08+j*.007; double a=t-start;if(a<0||a>.6)continue;constexpr std::array<double,6> individual{{-7,3,-2,8,-5,1}};double f=440*std::exp2((chords[c][j]-69+(cents+individual[j])* .01)/12.);double amp=std::min(a/.002,1.)*std::exp(-a/1.1);
 for(int h=1;h<=8;++h){double weight=1./std::pow(double(h),1.2);if(timbre==1)weight*=h%2?.15:1.;if(timbre==2)weight*=std::exp(-h*h*.2*a);val+=.08/chords[c].size()*weight*amp*std::sin(2*pi*f*h*std::sqrt(1.+(.00002+.00002*j)*h*h)*a+h*.391+j*.76);}}
 input[n]=float(val);}
 for(int at=0;at<sr;at+=block){flysnapper::pitch::Output out;det->process(input.data()+at,std::min(block,sr-at),out);for(size_t k=0;k<out.size;++k){auto &e=out.events[k];if(!e.pitchChange)std::cout<<c<<','<<cents<<','<<timbre<<','<<dynamics<<','<<at+e.offset<<','<<(e.on?'N':'F')<<','<<e.note<<'\n';}}
 }
}
