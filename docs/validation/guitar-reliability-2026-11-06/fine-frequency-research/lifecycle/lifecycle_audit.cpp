#include <array>
#include <memory>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstdint>
#define private public
#include "Pitch/TimeDomainPursuit.h"
#undef private
#include "Source/Pitch/TimeDomainPursuit.cpp"
int main(){
 using namespace flysnapper::pitch;
 auto pursuit=std::make_unique<TimeDomainPursuit>();pursuit->prepare(12000,512);auto& d=*pursuit->data;
 uint32_t random=19837;auto next=[&](){random=1664525*random+1013904223;return random;};
 double crossMax=0,inverseMax=0,basisMax=0,symmetryMax=0;size_t checks=0;int maskStates=0;
 auto audit=[&](int n){const auto& m=d.model(n);
  for(int ca=0;ca<16;++ca){
   if(m.valid[size_t(ca)])for(int k=0;k<544;++k){const bool cosine=ca>=8;const double expected=((cosine?std::cos(m.phase[size_t(ca)]*k):std::sin(m.phase[size_t(ca)]*k))-m.mean[size_t(ca)])*m.weight[size_t(ca)];const double value=k<512?m.basis[size_t(ca*512+k)]:m.continuation[size_t(ca*32+k-512)];basisMax=std::max(basisMax,std::abs(value-expected));++checks;}
   for(int cb=0;cb<16;++cb){double unity=0;for(int j=0;j<16;++j)unity+=(d.cross(n,ca,n,j)+(ca==j?.006*512:0))*m.inv[size_t(j*16+cb)];inverseMax=std::max(inverseMax,std::abs(unity-(ca==cb?1.:0.)));++checks;}
  }
  for(int pair=0;pair<192;++pair){const int other=int(next()%73),ca=int(next()%16),cb=int(next()%16);const auto& b=d.model(other);double value=0;
   for(int i=0;i<512;++i)value+=double(m.basis[size_t(ca*512+i)])*b.basis[size_t(cb*512+i)];crossMax=std::max(crossMax,std::abs(value-d.cross(n,ca,other,cb)));symmetryMax=std::max(symmetryMax,std::abs(double(d.cross(n,ca,other,cb))-d.cross(other,cb,n,ca)));++checks;}
 };
 for(int trial=0;trial<24;++trial){const int n=12+int(next()%49),m=12+int(next()%49);const double f=440*std::exp2((n+28-69+(int(next()%97)-48)*.01)/12.);if(!d.rebuildModel(n,f))return 2;
  const double fm=440*std::exp2((m+28-69+(int(next()%97)-48)*.01)/12.);if(!d.rebuildModel(m,fm))return 3;
  for(int mask=0;mask<4;++mask){d.fineUsed.fill(false);d.fineUsed[size_t(n)]=mask&1;d.fineUsed[size_t(m)]=mask&2;audit(n);audit(m);++maskStates;}
 }
 const auto nominalBefore=d.nominalGram, fineNominalBefore=d.fineToNominalGram, fineBefore=d.gram;
 pursuit->reset();if(d.nominalGram!=nominalBefore||d.fineToNominalGram!=fineNominalBefore||d.gram!=fineBefore)return 4;
 if(std::any_of(d.fineCacheValid.begin(),d.fineCacheValid.end(),[](bool b){return b;})||std::any_of(d.fineUsed.begin(),d.fineUsed.end(),[](bool b){return b;}))return 5;
 for(int n=0;n<73;++n)audit(n);
 // A newly rebuilt fine row must pair with nominal models when old caches were invalidated.
 if(!d.rebuildModel(12,83.1))return 6;d.fineUsed[12]=true;audit(12);audit(36);
 d.retuneObserved(nullptr);if(d.fineUsed[12])return 7;
 TimeDomainPursuit::SpectralEvidence empty;d.fineUsed[12]=true;d.retuneObserved(&empty);if(d.fineUsed[12])return 8;
 std::cout<<"cross_abs_max="<<crossMax<<" inverse_residual_max="<<inverseMax<<" basis_abs_max="<<basisMax<<" symmetry_abs_max="<<symmetryMax<<" mask_states="<<maskStates<<" scalar_checks="<<checks<<" reset_large_tables_unchanged=1 null_empty_fallback_nominal=1\n";
 return crossMax<.0004&&inverseMax<.00001&&basisMax<.00001&&symmetryMax==0?0:1;
}
