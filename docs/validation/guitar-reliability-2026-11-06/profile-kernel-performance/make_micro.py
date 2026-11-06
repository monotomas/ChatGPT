from pathlib import Path
b=Path(__file__).parent
old=(b/'baseline-Source/Pitch/TimeDomainPursuit.cpp').read_text();new=(b/'fast-Source/Pitch/TimeDomainPursuit.cpp').read_text()
def region(text,marker):
 start=text.index(marker);end=text.index('\n        }\n        if(iteration==0)',start);return text[start:end]
oldregion=region(old,'            for(const auto& profile:data->profiles){')
newregion=region(new,'            unsigned validMask=0;')
prefix=r'''
// Private mathematical/numeric microbench; no detector, labels or audio.
#include <algorithm>
#include <array>
#include <memory>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <ctime>
#define private public
#include "Pitch/TimeDomainPursuit.h"
#undef private
#include "Pitch/TimeDomainPursuit.cpp"
using namespace flysnapper::pitch;
struct Input {std::array<double,columns> correlation{};std::array<double,8> observed{},phaseSin{},phaseCos{};};
struct Result {double score=0;std::array<double,columns> shape{};};
static uint64_t nowThread(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return uint64_t(t.tv_sec)*1000000000ULL+uint64_t(t.tv_nsec);}
static uint64_t rng=0x512e66adf9212431ULL;
static double draw(){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return (double(rng>>11)/9007199254740992.)*2-1;}
#ifdef FLYSNAPPER_PROFILE_SCORE_AUDIT
static uint64_t auditedScores=0,auditedCandidates=0,auditedCompressedDifferences=0,auditedFinalDifferences=0,auditedExtraRechecks=0;
static double auditedMaxRelative=0;
extern "C" void flysnapperScoreAudit(double approximate,double reference){++auditedScores;if(reference!=0)auditedMaxRelative=std::max(auditedMaxRelative,std::abs(approximate-reference)/std::abs(reference));}
extern "C" void flysnapperChoiceAudit(int approximate,int reference,int final,int rechecks){++auditedCandidates;auditedCompressedDifferences+=approximate!=reference;auditedFinalDifferences+=final!=reference;auditedExtraRechecks+=std::max(0,rechecks-1);}
#endif
'''
def function(name,body):
 return r'''
Result '''+name+r'''(TimeDomainPursuit::Data* data,int note,const Input& input){
 const auto& model=data->models[size_t(note)];const int samples=data->samples;
 const auto& correlation=input.correlation;const auto& observed=input.observed;const auto& phaseSin=input.phaseSin;const auto& phaseCos=input.phaseCos;
 auto crossShape=[&](int a,const std::array<double,columns>& x,int b,const std::array<double,columns>& y){double result=0;for(int i=0;i<columns;++i)for(int j=0;j<columns;++j)result+=x[size_t(i)]*data->cross(a,i,b,j)*y[size_t(j)];return result;};
 int best=-1;double bestScore=0;std::array<double,columns> bestShape{};
 do {
'''+body+r'''
 } while(false);
 return {bestScore,bestShape};
}
'''
tail=r'''
int main(){
 TimeDomainPursuit pursuit;pursuit.prepare(12000,512);auto* data=pursuit.data.get();
 uint64_t cacheChecks=0,cacheDifferences=0;
 for(unsigned mask=0;mask<256;++mask)for(size_t p=0;p<27;++p){double power=0;
  for(int h=0;h<8;++h)if(mask&(1u<<h))power+=double(data->profiles[p][size_t(h)])*data->profiles[p][size_t(h)];
  const double expected=power>0?1/std::sqrt(power):0.;++cacheChecks;cacheDifferences+=std::memcmp(&expected,&data->profileScales[mask][p],sizeof(double))!=0;
 }
 const auto originalProfiles=data->profiles;uint64_t comparisons=0,differences=0,scoreBitDifferences=0,shapeBitDifferences=0;
 double largestScoreError=0;std::array<uint64_t,3> byFamily{},familyDifferences{};
 std::vector<Input> normal;std::vector<int> normalNotes;
 for(int family=0;family<3;++family){
  data->profiles=originalProfiles;
  if(family>0)for(size_t p=1;p<27;++p){data->profiles[p]=data->profiles[0];
   if(family==2)for(size_t k=0;k<p;++k)data->profiles[p][7]=std::nextafter(data->profiles[p][7],std::numeric_limits<float>::infinity());}
  for(unsigned mask=0;mask<256;++mask)for(size_t p=0;p<27;++p){double power=0;for(int h=0;h<8;++h)if(mask&(1u<<h))power+=double(data->profiles[p][size_t(h)])*data->profiles[p][size_t(h)];data->profileScales[mask][p]=power>0?1/std::sqrt(power):0;}
  for(int trial=0;trial<3000;++trial){
   const int note=trial%73;const auto& model=data->models[size_t(note)];Input input;std::array<double,columns> beta{};double power=0;
   for(int h=0;h<8;++h)if(model.valid[size_t(h)]){beta[size_t(h)]=draw();beta[size_t(h+8)]=draw();if(trial%4==0&&h>0)beta[size_t(h)]=beta[size_t(h+8)]=0;
    const double magnitude=std::hypot(beta[size_t(h)],beta[size_t(h+8)]);input.observed[size_t(h)]=magnitude*model.weight[size_t(h)];power+=input.observed[size_t(h)]*input.observed[size_t(h)];
    if(magnitude>1e-15){input.phaseSin[size_t(h)]=beta[size_t(h)]/magnitude;input.phaseCos[size_t(h)]=beta[size_t(h+8)]/magnitude;}}
   for(auto& value:input.observed)value/=std::sqrt(power);
   for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)input.correlation[size_t(r)]+=(data->cross(note,r,note,c)+(r==c?ridge*data->samples:0))*beta[size_t(c)];
   const auto old=originalKernel(data,note,input),fast=fastKernel(data,note,input);const bool s=std::memcmp(&old.score,&fast.score,sizeof(double))!=0,a=std::memcmp(old.shape.data(),fast.shape.data(),sizeof(old.shape))!=0;
   ++comparisons;++byFamily[size_t(family)];differences+=s||a;familyDifferences[size_t(family)]+=s||a;scoreBitDifferences+=s;shapeBitDifferences+=a;largestScoreError=std::max(largestScoreError,std::abs(old.score-fast.score));
   if(family==0){normal.push_back(input);normalNotes.push_back(note);}
  }
 }
 data->profiles=originalProfiles;for(unsigned mask=0;mask<256;++mask)for(size_t p=0;p<27;++p){double power=0;for(int h=0;h<8;++h)if(mask&(1u<<h))power+=double(data->profiles[p][size_t(h)])*data->profiles[p][size_t(h)];data->profileScales[mask][p]=power>0?1/std::sqrt(power):0;}
 volatile double checksum=0;
#ifdef FLYSNAPPER_PROFILE_SCORE_AUDIT
 constexpr int loops=0;
#else
 constexpr int loops=100000;
#endif
 auto begin=nowThread();for(int i=0;i<loops;++i)checksum+=originalKernel(data,normalNotes[size_t(i)%normal.size()],normal[size_t(i)%normal.size()]).score;const uint64_t oldNs=nowThread()-begin;
 begin=nowThread();for(int i=0;i<loops;++i)checksum+=fastKernel(data,normalNotes[size_t(i)%normal.size()],normal[size_t(i)%normal.size()]).score;const uint64_t fastNs=nowThread()-begin;
 std::cout<<std::setprecision(17)<<"{\n\"cached_scale_bit_checks\":"<<cacheChecks<<",\n\"cached_scale_bit_differences\":"<<cacheDifferences<<",\n\"kernel_comparisons\":"<<comparisons<<",\n\"kernel_bit_differences\":"<<differences<<",\n\"score_bit_differences\":"<<scoreBitDifferences<<",\n\"shape_bit_differences\":"<<shapeBitDifferences<<",\n\"max_winner_score_difference\":"<<largestScoreError<<",\n\"families_normal_duplicate_adjacent_float_comparisons\":["<<byFamily[0]<<','<<byFamily[1]<<','<<byFamily[2]<<"],\n\"families_bit_differences\":["<<familyDifferences[0]<<','<<familyDifferences[1]<<','<<familyDifferences[2]<<"],\n\"timing_loops\":"<<loops<<",\n\"original_thread_ns\":"<<oldNs<<",\n\"fast_thread_ns\":"<<fastNs<<",\n\"checksum\":"<<checksum<<"\n}\n";
#ifdef FLYSNAPPER_PROFILE_SCORE_AUDIT
 std::cerr<<std::setprecision(17)<<"{\n\"score_calls\":"<<auditedScores<<",\n\"candidate_calls\":"<<auditedCandidates<<",\n\"compressed_argmax_differences\":"<<auditedCompressedDifferences<<",\n\"final_argmax_differences\":"<<auditedFinalDifferences<<",\n\"extra_original_rechecks\":"<<auditedExtraRechecks<<",\n\"max_relative_score_error\":"<<auditedMaxRelative<<"\n}\n";
#endif
 return cacheDifferences||differences?1:0;
}
'''
(b/'micro.cpp').write_text(prefix+function('originalKernel',oldregion)+function('fastKernel',newregion)+tail)
