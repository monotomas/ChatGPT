#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <numeric>
#define private public
#include "Pitch/PolyphonicTracker.h"
#undef private
#include "Pitch/PolyphonicTracker.cpp"
using namespace flysnapper::pitch;
double referenceCross(const Model& a,int ca,const Model& b,int cb){
 if(!a.valid[size_t(ca)]||!b.valid[size_t(cb)])return 0;
 const auto minus=sums(a.omega[size_t(ca)]-b.omega[size_t(cb)],a.count);
 const auto plus=sums(a.omega[size_t(ca)]+b.omega[size_t(cb)],a.count);
 const bool ac=ca>=harmonics,bc=cb>=harmonics;double value=0;
 if(ac&&bc)value=.5*(minus.cosine+plus.cosine);
 else if(!ac&&!bc)value=.5*(minus.cosine-plus.cosine);
 else if(!ac&&bc)value=.5*(plus.sine+minus.sine);
 else value=.5*(plus.sine-minus.sine);
 return (value-a.count*a.mean[size_t(ca)]*b.mean[size_t(cb)])*a.weight[size_t(ca)]*b.weight[size_t(cb)];
}
int main(){
 PolyphonicTracker tracker;tracker.prepare(12000);auto& d=*tracker.data;
 std::array<double,maxJoint*maxJoint> matrix{};std::array<double,maxJoint> rhs{},solution{};
 double maxError=0;int cases=0;const std::array<double,6> frequencies{{82.41,123.47,164.81,196.,246.94,329.63}};
 for(int count:{128,192,256,384,512})for(int used=1;used<=6;++used){
  for(int j=0;j<used;++j)d.models[size_t(j)].prepare(frequencies[size_t(j)]*(1+.00017*j),12000,count,j%2?.0001:0);
  for(int n=0;n<count;++n)d.wave[size_t(n)]=float(.1*std::sin(.073*n)+.031*std::cos(.113*n)+.002*std::sin(.793*n));
  if(!d.fit(d.models.data(),used,d.wave.data(),count)){std::cerr<<"optimized solve failed\n";return 1;}
  const int dimensions=used*columns;rhs.fill(0);solution.fill(0);
  for(int j=0;j<used;++j)for(int c=0;c<columns;++c)if(d.models[size_t(j)].valid[size_t(c)])
   for(int n=0;n<count;++n)rhs[size_t(j*columns+c)]+=double(d.models[size_t(j)].basis[size_t(c*PolyphonicTracker::maximumSamples+n)])*d.wave[size_t(n)];
  for(int r=0;r<dimensions;++r)for(int c=0;c<=r;++c)
   matrix[size_t(r*maxJoint+c)]=referenceCross(d.models[size_t(r/columns)],r%columns,d.models[size_t(c/columns)],c%columns)+(r==c?count*.002:0);
  if(!solve(matrix,rhs,solution,dimensions)){std::cerr<<"reference solve failed\n";return 1;}
  for(int j=0;j<used;++j)for(int c=0;c<columns;++c)
   maxError=std::max(maxError,std::abs(d.models[size_t(j)].coefficient[size_t(c)]-solution[size_t(j*columns+c)]));
  ++cases;
 }
 std::cout<<"cases="<<cases<<" max_coefficient_error="<<maxError<<'\n';return maxError>1e-10;
}
