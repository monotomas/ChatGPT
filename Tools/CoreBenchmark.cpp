// SPDX-License-Identifier: AGPL-3.0-or-later
// Standalone core CPU kernel benchmark. This does NOT measure pitch detection,
// audio-device, VST3 wrapper, host routing or end-to-end musical latency.
#include "Core/MidiEngine.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace flysnapper;
namespace {
enum class Load {Idle, Musical, Dense};
const char* name(Load l){return l==Load::Idle?"idle":l==Load::Musical?"musical_4_note_events":"dense_2048_note_events";}
void kernel(Engine& e,Output& o,int samples,int iteration,Load load) noexcept {
    o.clear();
    auto settings=e.settings();settings.mode=Mode::FixedScale;settings.scale=1;
    e.setSettings(settings);
    if((iteration%32)==0){
        e.clearChord();for(auto n:{48,52,55})e.chordNoteOn(n);e.refreshChord();
    } else e.refreshChord();
    if(load==Load::Musical){
        int n=48+iteration%24;
        e.beginTimestamp(2);e.noteOn(Source::External,1,n,100,0,o);
        e.noteOn(Source::Generated,1,n+7,95,0,o);
        e.beginTimestamp(0);e.noteOff(Source::External,1,n,0,samples-1,o);
        e.noteOff(Source::Generated,1,n+7,0,samples-1,o);
    } else if(load==Load::Dense){
        // Two rounds of 512 active input bindings: no overflow is used as a
        // shortcut. Each burst's identical offset is explicitly polyphonic.
        for(int round=0;round<2;++round){
            e.beginTimestamp(512);
            for(int i=0;i<512;++i)e.noteOn(Source(i%2),1+i/128,i%128,100,round*samples/2,o);
            e.beginTimestamp(0);
            for(int i=0;i<512;++i)e.noteOff(Source(i%2),1+i/128,i%128,0,((round+1)*samples/2)-1,o);
        }
    }
    const auto snapshot=e.snapshot();(void)packSnapshot(snapshot);
}
}
int main(int argc,char** argv){
    const int iterations=argc>1?std::max(100,std::atoi(argv[1])):2000;
    std::cout<<"Core CPU benchmark; sample rate 48000 Hz; iterations "<<iterations<<"; warmup 200 per case.\n";
    std::cout<<"load,buffer_samples,block_deadline_us,median_us,p95_us,p99_us,max_us,deadline_misses\n";
    for(auto load:{Load::Idle,Load::Musical,Load::Dense})for(int samples:{32,64,128}) {
        Engine e;Output output;std::vector<double> times(size_t(iterations),0.0);
        for(int i=0;i<200;++i)kernel(e,output,samples,i,load);
        size_t misses=0;const double deadline=1e6*samples/48000.;
        for(int i=0;i<iterations;++i){
            auto begin=std::chrono::steady_clock::now();
            kernel(e,output,samples,i,load);
            auto end=std::chrono::steady_clock::now();
            times[size_t(i)]=std::chrono::duration<double,std::micro>(end-begin).count();
            if(times[size_t(i)]>deadline)++misses;
            if(e.activeVoices()!=0||e.overloaded()||output.overflow){std::cerr<<"Invalid benchmark state\n";return 2;}
        }
        std::sort(times.begin(),times.end());
        const auto percentile=[&](double p){return times[std::min(times.size()-1,size_t(p*(times.size()-1)))];};
        std::cout<<name(load)<<','<<samples<<','<<std::fixed<<std::setprecision(3)<<deadline<<','
            <<percentile(.5)<<','<<percentile(.95)<<','<<percentile(.99)<<','<<times.back()<<','<<misses<<'\n';
    }
}
