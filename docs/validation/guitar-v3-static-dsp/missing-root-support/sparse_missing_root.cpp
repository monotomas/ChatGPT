#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
using namespace flysnapper::pitch;
constexpr double pi=3.14159265358979323846;
int main(){
    std::cout<<"root,low_ratio,on,note,voice_id,sample\n";
    for(int root:{28,40,52,64})for(double ratio:{0.,.002,.01,.05}){
        PitchDetector detector;detector.prepare(48000);Settings settings;
        settings.polyphonic=true;settings.polyBend=false;detector.configure(settings);
        const double f=440*std::pow(2.,(root-69.)/12.);
        std::vector<float> wave(48000,0);
        for(int i=0;i<36000;++i){
            const double t=double(i)/48000,attack=std::min(1.,t/.002);
            const auto partial=[&](int h){return std::sin(2*pi*f*h*t+.17*h);};
            wave[size_t(i)]=float(attack*(.16*partial(5)+.11*partial(7)+.16*ratio*(partial(2)+partial(3))));
        }
        Output out;
        for(int at=0;at<int(wave.size());at+=64){
            out.clear();detector.process(wave.data()+at,std::min(64,int(wave.size())-at),out);
            for(size_t j=0;j<out.size;++j){const auto& e=out.events[j];if(e.pitchChange)continue;
                std::cout<<root<<','<<ratio<<','<<e.on<<','<<e.note<<','<<e.voiceId<<','<<at+e.offset<<'\n';
            }
        }
    }
}
