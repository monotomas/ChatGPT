// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <array>
#include <memory>
#include <iostream>
#include <cmath>
#define private public
#include "Pitch/PolyphonicTracker.h"
#undef private
#include "Pitch/PolyphonicTracker.cpp"
int main(){
 using namespace flysnapper::pitch;PolyphonicTracker t;t.prepare(12000);auto&d=*t.data;
 const int n=384,used=2;const double span=(n-1)/12000.;
 for(int j=0;j<2;++j){
  d.models[j].prepare(j?196.3:146.8,j?-35:44,12000,n,j?.0004:.00015);
  for(int c=0;c<16;++c)d.models[j].coefficient[c]=.04*std::sin(c*1.7+j*.9);
  d.trials[j]=d.models[j];d.trials[j].prepare(d.models[j].frequency+(j?.35:-.25),d.models[j].chirp+(j?-8:10),12000,n,d.models[j].stiffness+.00002);
 }
 for(int a=0;a<n;++a)d.wave[a]=float(d.trials[0].value(a)+d.trials[1].value(a));
 d.fit(d.models.data(),used,n);d.residualFor(d.models.data(),used,n,true);
 for(int p=0;p<6;++p)d.precision[p]=1e-9;
 d.derivatives(used,n);auto analytic=d.gradient;double maximum=0;
 for(int p=0;p<6;++p){double costs[2];const double epsilon=p%3==2?.00003:.003;
  for(int sign=0;sign<2;++sign){
   for(int j=0;j<2;++j){
    d.trials[j]=d.models[j];double f=d.models[j].frequency,v=d.models[j].chirp,beta=f*d.models[j].stiffness;
    if(p/3==j){const double step=(sign?1:-1)*epsilon;if(p%3==0)f+=step;else if(p%3==1)v+=step/span;else beta+=step;}
    d.trials[j].prepare(f,v,12000,n,beta/f);
   }
   d.fit(d.trials.data(),used,n);costs[sign]=d.objective(d.trials.data(),used,n,d.residualFor(d.trials.data(),used,n,false));
  }
  const double numerical=(costs[1]-costs[0])/(2*epsilon),error=std::abs(numerical+2*analytic[p]);maximum=std::max(maximum,error);
  std::cout<<p<<" numerical="<<numerical<<" analytic="<<-2*analytic[p]<<" absdiff="<<error<<'\n';
 }
 std::cout<<"Maximum absolute gradient difference "<<maximum<<'\n';return maximum<.0001?0:1;
}
