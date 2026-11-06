#include <fstream>
#include <cstdint>
#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <cstdlib>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>

namespace {uint64_t scoreCalls=0,candidateCalls=0,compressedMismatches=0,finalMismatches=0,extraRechecks=0,zeroReferenceNonzero=0;double maxAbsolute=0,maxRelative=0;
struct AuditReport {~AuditReport(){std::ofstream out(FLYSNAPPER_AUDIT_PATH);out<<std::setprecision(17)<<"{\n\"score_calls\":"<<scoreCalls<<",\n\"candidate_calls\":"<<candidateCalls<<",\n\"compressed_argmax_differences\":"<<compressedMismatches<<",\n\"final_argmax_differences\":"<<finalMismatches<<",\n\"extra_original_rechecks\":"<<extraRechecks<<",\n\"reference_zero_approx_nonzero\":"<<zeroReferenceNonzero<<",\n\"max_absolute_score_error\":"<<maxAbsolute<<",\n\"max_relative_score_error\":"<<maxRelative<<"\n}\n";}} reporter;
}
extern "C" void flysnapperScoreAudit(double approximate,double reference){++scoreCalls;const double error=std::abs(approximate-reference);maxAbsolute=std::max(maxAbsolute,error);if(reference!=0)maxRelative=std::max(maxRelative,error/std::abs(reference));else zeroReferenceNonzero+=approximate!=0;}
extern "C" void flysnapperChoiceAudit(int approximate,int reference,int final,int rechecks){++candidateCalls;compressedMismatches+=approximate!=reference;finalMismatches+=final!=reference;extraRechecks+=std::max(0,rechecks-1);}

int main(int argc,char**argv){
 constexpr double pi=3.14159265358979323846; constexpr int sr=48000, block=64;
 const std::vector<std::vector<int>> chords{{40},{45},{52},{57},{64},{40,52},{45,64},{48,52,55},{52,55,59},{40,47,52,55,59,64},{40,45,50,55,59,64}};

 std::cout<<"case,cents,timbre,dynamics,sample,type,note\n";
 std::cerr<<"case,cents,timbre,dynamics,callbacks,deadline_misses,cpu_median_us,cpu_p95_us,cpu_p99_us,cpu_max_us\n";
 for(size_t c=0;c<chords.size();++c)for(int cents: {-40,-20,0,20,40})for(int timbre: {0,1,2})for(int dynamics:{0,1}){
 if(argc>1&&int(c)!=std::atoi(argv[1]))continue;
 auto det=std::make_unique<flysnapper::pitch::PitchDetector>();det->prepare(sr); flysnapper::pitch::Settings s;s.polyphonic=true;s.inputDynamics.enabled=dynamics;det->configure(s);
 std::vector<float> input(sr); for(int n=0;n<sr;++n){double t=double(n)/sr;double val=0;
 for(size_t j=0;j<chords[c].size();++j){double start=.08+j*.007; double a=t-start;if(a<0||a>.6)continue;double f=440*std::exp2((chords[c][j]-69+cents*.01)/12.);double amp=std::min(a/.002,1.)*std::exp(-a/1.1);
 for(int h=1;h<=8;++h){double weight=1./std::pow(double(h),1.2);if(timbre==1)weight*=h%2?.15:1.;if(timbre==2)weight*=std::exp(-h*h*.2*a);val+=.08/chords[c].size()*weight*amp*std::sin(2*pi*f*h*a+h*.391+j*.76);}}
 input[n]=float(val);}
 std::vector<double> timings;int deadlineMisses=0;
 for(int at=0;at<sr;at+=block){flysnapper::pitch::Output out;const auto before=std::chrono::steady_clock::now();det->process(input.data()+at,std::min(block,sr-at),out);const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-before).count();timings.push_back(us);deadlineMisses+=us>1e6*std::min(block,sr-at)/sr;for(size_t k=0;k<out.size;++k){auto &e=out.events[k];if(!e.pitchChange)std::cout<<c<<','<<cents<<','<<timbre<<','<<dynamics<<','<<at+e.offset<<','<<(e.on?'N':'F')<<','<<e.note<<'\n';}}
 std::sort(timings.begin(),timings.end());
 std::cerr<<c<<','<<cents<<','<<timbre<<','<<dynamics<<','<<timings.size()<<','<<deadlineMisses;
 for(double q:{.5,.95,.99,1.})std::cerr<<','<<timings[size_t(std::ceil(q*(timings.size()-1)))];
 std::cerr<<'\n';
 }
}
