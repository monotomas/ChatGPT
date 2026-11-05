#include "Pitch/PitchDetector.h"
#include "Pitch/InputDynamics.h"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>
using namespace flysnapper::pitch;
template<class T,class=void>struct Inner:std::false_type{};
template<class T>struct Inner<T,std::void_t<decltype(std::declval<T>().inputDynamics)>>:std::true_type{};
template<class T>void configureInner(T& s,const InputDynamics::Settings& d){if constexpr(Inner<T>::value)s.inputDynamics=d;}
constexpr double pi=3.14159265358979323846;
int main(){
 std::cout<<"rate,range,sample,type,note,pitch\n";
 for(int rate:{44100,48000,96000})for(int range:{2,12}){
  auto d=std::make_unique<PitchDetector>();d->prepare(rate);Settings s;s.monoBend=true;s.bendRangeSemitones=float(range);
  InputDynamics dynamics(rate);auto ds=dynamics.settings();ds.enabled=true;dynamics.configure(ds);configureInner(s,ds);d->configure(s);
  const double frequency=440*std::pow(2.,(57.3-69)/12.);Output out;
  for(int n=0;n<int(rate*1.3);++n){const double time=double(n)/rate;double value=0;
   if(time<1.05)for(int pick=0;pick<3;++pick){const double age=time-.32*pick;if(age<0)continue;const double phase=2*pi*frequency*age+1.6*pick;
    value+=.23*std::min(1.,age/.001)*std::exp(-6*age)*(std::sin(phase)+.25*std::sin(2*phase+.2)+.09*std::sin(3*phase+.6));}
   float x=float(value);
   if constexpr(!Inner<Settings>::value){x=dynamics.processSample(x);s.allowNewNotes=dynamics.allowsNewNotes();d->configure(s);}
   out.clear();d->process(&x,1,out);
   for(size_t k=0;k<out.size;++k){const auto&e=out.events[k];std::cout<<rate<<','<<range<<','<<n<<','<<(e.pitchChange?'P':e.on?'N':'F')<<','<<e.note<<','<<e.pitch<<'\n';}
  }
 }
}
