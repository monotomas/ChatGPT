#include <algorithm>
#include <array>
#include <memory>
#include <iostream>
#include <cmath>
#define private public
#include "Pitch/PolyphonicTracker.h"
#undef private
#include "Pitch/PolyphonicTracker.cpp"
int main(){using namespace flysnapper::pitch;PolyphonicTracker t;t.prepare(12000);auto&d=*t.data;const int n=384,used=2;const double span=(n-1)/12000.;for(int j=0;j<2;++j){d.models[j].prepare(j?196.3:146.8,j?-35:44,12000,n);for(int c=0;c<16;++c)d.models[j].coefficient[c]=.04*std::sin(c*1.7+j*.9);d.trials[j]=d.models[j];d.trials[j].prepare(d.models[j].frequency+(j?.35:-.25),d.models[j].chirp+(j?-8:10),12000,n);}for(int a=0;a<n;++a){d.weights[a]=a<100?.1f:1.f;d.rootWeights[a]=std::sqrt(d.weights[a]);d.wave[a]=float(d.trials[0].value(a)+d.trials[1].value(a));}d.fit(d.models.data(),used,n);d.residualFor(d.models.data(),used,n,true);for(int p=0;p<4;++p)d.precision[p]=1e-9;d.derivatives(used,n);auto analytic=d.gradient;for(int p=0;p<4;++p){double costs[2];for(int sign=0;sign<2;++sign){for(int j=0;j<2;++j){d.trials[j]=d.models[j];double f=d.models[j].frequency,v=d.models[j].chirp;if(p/2==j){if(p%2)v+=(sign?1:-1)*.003/span;else f+=(sign?1:-1)*.003;}d.trials[j].prepare(f,v,12000,n);}d.fit(d.trials.data(),used,n);costs[sign]=d.objective(d.trials.data(),used,n,d.residualFor(d.trials.data(),used,n,false));}const double numerical=(costs[1]-costs[0])/.006;std::cout<<p<<" numerical="<<numerical<<" analytic="<<-2*analytic[p]<<" absdiff="<<std::abs(numerical+2*analytic[p])<<'\n';}}
