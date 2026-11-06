#include <array>
#include <memory>
#include <cstdint>
#include <cstddef>
#define private public
#include "Pitch/PitchDetector.h"
#undef private
#include <algorithm>
#include <cstdlib>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <ctime>
uint64_t threadNow(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return uint64_t(t.tv_sec)*1000000000ULL+uint64_t(t.tv_nsec);}
int main(int argc,char**argv){
 constexpr double pi=3.14159265358979323846; constexpr int sr=48000, block=64;
 const std::vector<std::vector<int>> chords{{40},{45},{52},{57},{64},{40,52},{45,64},{48,52,55},{52,55,59},{40,47,52,55,59,64},{40,45,50,55,59,64}};

 std::cout<<"case,cents,timbre,dynamics,sample,type,note\n";
 std::cerr<<"case,cents,timbre,dynamics,wall_deadlines,thread_deadlines,wall_p99_us,wall_max_us,thread_p99_us,thread_max_us,process_thread_us,analyse_calls,analyse_thread_us,profile_candidates,profile_thread_us\n";
 for(size_t c=0;c<chords.size();++c)for(int cents: {-40,-20,0,20,40})for(int timbre: {0,1,2})for(int dynamics:{0,1}){
 if(argc>1&&int(c)!=std::atoi(argv[1]))continue;
 auto det=std::make_unique<flysnapper::pitch::PitchDetector>();det->prepare(sr); flysnapper::pitch::Settings s;s.polyphonic=true;s.inputDynamics.enabled=dynamics;det->configure(s);
 std::vector<float> input(sr); for(int n=0;n<sr;++n){double t=double(n)/sr;double val=0;
 for(size_t j=0;j<chords[c].size();++j){double start=.08+j*.007; double a=t-start;if(a<0||a>.6)continue;double f=440*std::exp2((chords[c][j]-69+cents*.01)/12.);double amp=std::min(a/.002,1.)*std::exp(-a/1.1);
 for(int h=1;h<=8;++h){double weight=1./std::pow(double(h),1.2);if(timbre==1)weight*=h%2?.15:1.;if(timbre==2)weight*=std::exp(-h*h*.2*a);val+=.08/chords[c].size()*weight*amp*std::sin(2*pi*f*h*a+h*.391+j*.76);}}
 input[n]=float(val);}
 std::vector<double> wall,cpu;int wallMiss=0,cpuMiss=0;double processCpu=0;
 for(int at=0;at<sr;at+=block){flysnapper::pitch::Output out;const uint64_t beforeCpu=threadNow();const auto beforeWall=std::chrono::steady_clock::now();det->process(input.data()+at,std::min(block,sr-at),out);const double w=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-beforeWall).count(),threadUs=(threadNow()-beforeCpu)/1000.;wall.push_back(w);cpu.push_back(threadUs);processCpu+=threadUs;wallMiss+=w>1e6*block/sr;cpuMiss+=threadUs>1e6*block/sr;for(size_t k=0;k<out.size;++k){auto &e=out.events[k];if(!e.pitchChange)std::cout<<c<<','<<cents<<','<<timbre<<','<<dynamics<<','<<at+e.offset<<','<<(e.on?'N':'F')<<','<<e.note<<'\n';}}
 flysnapper::pitch::TimeDomainPursuit::CpuProfile p;det->pursuit.copyCpuProfile(p);
 std::sort(wall.begin(),wall.end());std::sort(cpu.begin(),cpu.end());const size_t p99=size_t(std::ceil(.99*(wall.size()-1)));
 std::cerr<<c<<','<<cents<<','<<timbre<<','<<dynamics<<','<<wallMiss<<','<<cpuMiss<<','<<wall[p99]<<','<<wall.back()<<','<<cpu[p99]<<','<<cpu.back()<<','<<processCpu<<','<<p.analysisCalls<<','<<p.analyseNs/1000.<<','<<p.profileCandidates<<','<<p.profileNs/1000.<<'\n';
 }
}
