#include <array>
#include <memory>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>
#define private public
#include "Pitch/TimeDomainPursuit.h"
#undef private
#include "Source/Pitch/TimeDomainPursuit.cpp"
int main(){
 using namespace flysnapper::pitch;
 auto fresh=std::make_unique<TimeDomainPursuit>(),old=std::make_unique<TimeDomainPursuit>();fresh->prepare(12000,512);old->prepare(12000,512);
 auto& d=*old->data;for(int n=12;n<61;n+=2){if(!d.rebuildModel(n,440*std::exp2((n+28-69-.41)/12.)))return 2;d.fineUsed[size_t(n)]=true;}
 TimeDomainPursuit::SpectralEvidence e;e.independentFrequencyEvidence=true;e.frequencyMaximumMagnitude=e.maximumMagnitude=1;e.maximumHz=5400;
 for(int n=0;n<73;++n)e.frequencies[size_t(n)]=float(440*std::exp2((n+28-69)/12.));
 std::array<float,512> wave{};
 for(int midi:{40,47,55}){e.active[size_t(midi-28)]=true;const double f=440*std::exp2((midi-69+.27)/12.);
  for(int h=1;h<=8;++h)if(f*h<5400){e.frequencyPeaks[size_t(e.frequencyPeakCount++)]={float(f*h),float(1./h),0};e.peaks[size_t(e.count++)]={float(f*h),float(1./h),0};for(int i=0;i<512;++i)wave[size_t(i)]+=float(.02/h*std::sin(2*3.14159265358979323846*f*h*i/12000+h*.391));}}
 std::array<float,73> sa{},sb{},ca{},cb{};fresh->analyse(wave.data(),512,sa,ca,6,.65f,&e);old->analyse(wave.data(),512,sb,cb,6,.65f,&e);
 if(sa!=sb||ca!=cb)return 3;int used=0;
 for(int n=0;n<73;++n){if(fresh->data->fineUsed[size_t(n)]!=old->data->fineUsed[size_t(n)])return 4;if(fresh->data->fineUsed[size_t(n)])++used;if(fresh->data->model(n).frequency!=old->data->model(n).frequency)return 5;}
 std::array<float,32> pa{},pb{};const bool va=fresh->copyPrediction(pa),vb=old->copyPrediction(pb);if(va!=vb||pa!=pb)return 6;
 std::cout<<"different_prior_cache_same_current_evidence_identical_strength_confidence_prediction=1 selected_fine_models="<<used<<" prior_cache_models=25 reset_between_histories=0\n";return used>0?0:7;
}
