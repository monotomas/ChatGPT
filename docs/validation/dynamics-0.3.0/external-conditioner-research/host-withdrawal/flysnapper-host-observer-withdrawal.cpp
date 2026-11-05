#include "Pitch/NoiseConditioner.h"
#include <algorithm>
#include <cmath>
#include <iostream>
int main(){constexpr double pi=3.14159265358979323846;
 for(double rate:{44100.,48000.,96000.})for(double base:{50.,60.}){
  flysnapper::pitch::NoiseConditioner n(rate);for(int i=0;i<int(rate*3);++i){const double t=i/rate;const double x=.003*(std::sin(2*pi*base*t)+.4*std::sin(4*pi*base*t+.3)+.2*std::sin(6*pi*base*t+.7));n.processSample(float(x));}
  double after50ms=0;int lastNonzero=-1;
  for(int i=0;i<int(rate*.3);++i){const double y=n.processSample(0,true);if(std::abs(y)>1e-8)lastNonzero=i;if(i>=rate*.05)after50ms=std::max(after50ms,std::abs(y));}
  std::cout<<rate<<','<<base<<",last_above_minus160_db_ms,"<<1000*lastNonzero/rate<<",peak_after50ms,"<<after50ms<<'\n';if(after50ms>1e-8)return 1;
 }
}
