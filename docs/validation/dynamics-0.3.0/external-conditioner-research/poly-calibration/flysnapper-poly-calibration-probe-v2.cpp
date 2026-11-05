#include "Pitch/InputDynamics.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
constexpr double pi=3.1415926535897932384626433832795;
using D=flysnapper::pitch::InputDynamics;
struct Random {uint32_t value=0xf53e1a27;double next(){value=value*1664525u+1013904223u;return 2*double(value)/4294967296.-1;}};
int main(){
 const std::array<std::array<int,6>,6> chords{{{{40,47,52,55,59,64}},{{43,47,50,55,59,67}},{{45,52,57,60,64,69}},{{42,45,50,57,62,66}},{{41,48,53,57,60,65}},{{40,48,52,55,60,64}}}};
 std::cout<<"source,snr_db,gain,chord_seconds,success,failure,tonal_seconds,active_seconds,noise_db,soft_db,gate_db,release_ms,peak\n";
 for(double period:{2.5,1.2,.65})for(double snr:{36.,24.,12.,6.})for(double common:{.1,1.,3.}){
  D d(48000);d.startCalibration();Random random;double peak=0;const double noiseRms=.060621778*std::pow(10.,-snr/20.);
  for(int i=0;i<960000;++i){const double time=i/48000.;double x=random.next()*noiseRms*std::sqrt(3.)+noiseRms*.25*std::sin(2*pi*50*time);
   if(time>=3){const double playing=time-3;const int chord=int(playing/period)%int(chords.size());const double since=std::fmod(playing,period);
    for(int string=0;string<6;++string){const double local=since-string*.012;if(local<0)continue;const double cents=(string-2.5)*1.1;const double f0=440*std::pow(2.,(chords[size_t(chord)][size_t(string)]-69+cents/100)/12.);
     const double stiff=.000012*(1+string*.3),decay=.72+.12*string,attack=1-std::exp(-local/.0008);double tone=0;
     for(int h=1;h<=8;++h){const double hz=f0*h*std::sqrt((1+stiff*h*h)/(1+stiff));tone+=std::sin(2*pi*hz*local+(string+1)*.41*h+chord*.19)/(h*std::pow(double(h),.28))*std::exp(-local*(h-1)*.24);}
     x+=.035*attack*std::exp(-local/decay)*tone;
    }
   }
   x*=common;peak=std::max(peak,std::abs(x));d.processSample(float(x));
  }
  const auto s=d.calibrationStatus();const auto& r=s.result;
  std::cout<<"constructed_six_string_chords,"<<snr<<','<<common<<','<<period<<','<<r.success<<','<<int(r.failure)<<','<<r.tonalSeconds<<','<<r.usableSeconds<<','<<r.noiseFloorDb<<','<<r.softSignalDb<<','<<r.settings.gateThresholdDb<<','<<r.settings.gateReleaseMs<<','<<peak<<'\n';
 }
}
