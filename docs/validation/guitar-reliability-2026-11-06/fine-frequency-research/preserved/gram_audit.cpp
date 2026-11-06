#include <array>
#include <memory>
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <cmath>
#define private public
#include "Pitch/TimeDomainPursuit.h"
#undef private
#include "Source/Pitch/TimeDomainPursuit.cpp"
int main(){
    using namespace flysnapper::pitch;
    auto pursuit=std::make_unique<TimeDomainPursuit>();pursuit->prepare(12000,512);auto& d=*pursuit->data;
    uint32_t random=19837;auto next=[&](){random=1664525*random+1013904223;return random;};
    double crossMax=0,inverseMax=0,basisMax=0;std::vector<double> times;
    for(int trial=0;trial<24;++trial){
        const int n=12+int(next()%49);const double detune=(int(next()%97)-48)*.01;
        const double f=440*std::exp2((n+28-69+detune)/12.);
        auto start=std::chrono::steady_clock::now();if(!d.rebuildModel(n,f))return 2;
        times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
        const auto& m=d.models[size_t(n)];
        for(int ca=0;ca<16;++ca){
            if(m.valid[size_t(ca)])for(int k=0;k<544;++k){const bool cosine=ca>=8;const double expected=((cosine?std::cos(m.phase[size_t(ca)]*k):std::sin(m.phase[size_t(ca)]*k))-m.mean[size_t(ca)])*m.weight[size_t(ca)];const double value=k<512?m.basis[size_t(ca*512+k)]:m.continuation[size_t(ca*32+k-512)];basisMax=std::max(basisMax,std::abs(value-expected));}
            for(int cb=0;cb<16;++cb){double value=0;for(int i=0;i<512;++i)value+=double(m.basis[size_t(ca*512+i)])*m.basis[size_t(cb*512+i)];crossMax=std::max(crossMax,std::abs(value-d.cross(n,ca,n,cb)));
                double unity=0;for(int j=0;j<16;++j)unity+=(d.cross(n,ca,n,j)+(ca==j?.006*512:0))*m.inv[size_t(j*16+cb)];inverseMax=std::max(inverseMax,std::abs(unity-(ca==cb?1.:0.)));}
        }
        for(int pair=0;pair<96;++pair){const int other=int(next()%73),ca=int(next()%16),cb=int(next()%16);const auto& b=d.models[size_t(other)];double value=0;
            for(int i=0;i<512;++i)value+=double(m.basis[size_t(ca*512+i)])*b.basis[size_t(cb*512+i)];crossMax=std::max(crossMax,std::abs(value-d.cross(n,ca,other,cb)));}
    }
    TimeDomainPursuit::SpectralEvidence evidence;evidence.maximumMagnitude=1;evidence.maximumHz=5400;
    for(int midi=40;midi<=88;midi+=3)for(int h=1;h<=4;++h){const double f=440*std::exp2((midi-69+.27)/12.);if(f*h<5400)evidence.peaks[size_t(evidence.count++)]={float(f*h),float(1./h),0};}
    std::array<double,73> previous{};for(int n=0;n<73;++n)previous[size_t(n)]=d.models[size_t(n)].frequency;d.retuneObserved(&evidence);int changed=0;for(int n=0;n<73;++n)changed+=previous[size_t(n)]!=d.models[size_t(n)].frequency;
    std::sort(times.begin(),times.end());
    std::cout<<"cross_abs_max="<<crossMax<<" inverse_residual_max="<<inverseMax<<" basis_abs_max="<<basisMax<<" retuned_in_one_frame="<<changed<<" rebuild_us_median="<<times[times.size()/2]<<" rebuild_us_max="<<times.back()<<'\n';
    return crossMax<.0004&&inverseMax<.00001&&basisMax<.00001&&changed<=12?0:1;
}
