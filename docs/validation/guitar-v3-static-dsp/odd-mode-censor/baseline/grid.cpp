#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>
using namespace flysnapper::pitch;
constexpr double pi=3.14159265358979323846;
int main(){
 std::cout<<"moving,held,partials,decay,on,note,id,sample\n";
 for(int moving:{45,47,48,50,52})for(int partials:{3,4,5,6,8,12})for(double decay:{1.25}){
  PitchDetector d;d.prepare(48000);Settings s;s.polyphonic=true;s.polyBend=false;d.configure(s);
  double a=0,b=0;std::vector<float>w(55200);
  for(int n=0;n<43200;++n){const double t=n/48000.;int pitch=t<.4?moving:moving+12;
   a+=2*pi*440*std::pow(2.,(pitch-69.)/12.)/48000;
   b+=2*pi*440*std::pow(2.,(64.-69.)/12.)/48000;
   double x=0,y=0;for(int h=1;h<=partials;++h){x+=std::sin(h*a+.17*h)/std::pow(double(h),decay);y+=std::sin(h*b+.11*h)/std::pow(double(h),decay);}
   w[size_t(n)]=float(std::min(1.,t/.002)*std::exp(-.18*t)*(.15*x+.11*y));
  }
  Output o;for(int at=0;at<int(w.size());at+=64){o.clear();d.process(w.data()+at,std::min(64,int(w.size())-at),o);for(size_t j=0;j<o.size;++j){const auto&e=o.events[j];if(e.pitchChange)continue;std::cout<<moving<<",64,"<<partials<<','<<decay<<','<<e.on<<','<<e.note<<','<<e.voiceId<<','<<at+e.offset<<'\n';}}
 }
}
