#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace flysnapper::pitch;
namespace fs=std::filesystem;
constexpr double pi=3.14159265358979323846;
struct Reference{int note,on,off;};
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("CounterCapture <fresh-output-directory>");
    const fs::path dir=argv[1];fs::create_directories(dir);
    std::ofstream events(dir/"events.csv"),status(dir/"status.csv"),metrics(dir/"capture.csv"),refs(dir/"references.csv");
    events<<"case,root,family,mono_bend,sample,type,note,velocity,pitch,voice_id\n";
    status<<"case,sample,level_db,confidence,active,strongest,pitch,raw_rms_db,processed_db,gain_db,gate_open,admission_open\n";
    metrics<<"case,root,family,mono_bend,dynamics_enabled,bend_range,rate,block,source_frames,frames,events,overflow,active_at_end\n";
    refs<<"case,index,note,on_sample,off_sample\n";events<<std::setprecision(10);status<<std::setprecision(10);
    int completed=0;
    for(int root:{31,35,40,52})for(bool bend:{false,true})for(const std::string family:{"true_octave_up","true_octave_down","fretted_two_semitone_up_return","physical_damping"}){
        const std::string name=family+"_"+std::to_string(root)+(bend?"_bend":"_plain");
        std::vector<Reference> truth;
        if(family=="true_octave_up")truth={{root,9600,31200},{root+12,31200,52800}};
        else if(family=="true_octave_down")truth={{root+12,9600,31200},{root,31200,52800}};
        else if(family=="fretted_two_semitone_up_return")truth={{root,9600,26400},{root+2,26400,43200},{root,43200,60000}};
        else truth={{root,9600,36000}};
        const int dampingStart=truth.back().off,sourceFrames=dampingStart+192,total=sourceFrames+12000;
        std::vector<float> input(size_t(total),0);double phase=0;
        for(int n=9600;n<sourceFrames;++n){
            int pitch=truth.back().note;
            for(const auto& ref:truth)if(n>=ref.on&&n<ref.off){pitch=ref.note;break;}
            const double frequency=440*std::pow(2.,(pitch-69.)/12.);phase+=2*pi*frequency/48000;
            double value=0;for(int h=1;h<=12;++h)value+=std::sin(h*phase+.17*h)/std::pow(double(h),1.25);
            const double age=double(n-9600)/48000,attack=std::min(1.,age/.002),damping=n<dampingStart?1.:std::max(0.,1.-double(n-dampingStart)/192);
            input[size_t(n)]=float(.15*attack*std::exp(-.18*age)*damping*value);
        }
        auto detector=std::make_unique<PitchDetector>();detector->prepare(48000);Settings settings;settings.monoBend=bend;settings.bendRangeSemitones=12;settings.inputDynamics.enabled=true;detector->configure(settings);
        Output output;int count=0,overflow=0;
        for(int at=0;at<total;at+=64){const int size=std::min(64,total-at);output.clear();detector->process(input.data()+at,size,output);overflow+=output.overflow;
            for(size_t k=0;k<output.size;++k){const auto&e=output.events[k];if(e.offset<0||e.offset>=size)throw std::runtime_error("invalid event offset");
                events<<name<<','<<root<<','<<family<<','<<bend<<','<<at+e.offset<<','<<(e.pitchChange?'P':e.on?'N':'F')<<','<<e.note<<','<<e.velocity<<',';
                if(e.pitchChange)events<<e.pitch;events<<','<<e.voiceId<<'\n';++count;
            }
            const auto s=detector->status();const auto m=detector->inputDynamicsMeter();status<<name<<','<<at+size-1<<','<<s.levelDb<<','<<s.confidence<<','<<s.activeNotes<<','<<s.strongestNote<<','<<s.continuousPitch<<','<<m.rawRmsDb<<','<<m.processedDb<<','<<m.gainDb<<','<<m.gateOpen<<','<<m.admissionOpen<<'\n';
        }
        for(size_t i=0;i<truth.size();++i)refs<<name<<','<<i<<','<<truth[i].note<<','<<truth[i].on<<','<<truth[i].off<<'\n';
        metrics<<name<<','<<root<<','<<family<<','<<bend<<",1,12,48000,64,"<<sourceFrames<<','<<total<<','<<count<<','<<overflow<<','<<detector->status().activeNotes<<'\n';++completed;
    }
    std::cout<<"Completed "<<completed<<" predeclared physical counters.\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
