// SPDX-License-Identifier: AGPL-3.0-or-later
// Research-only causal variable-projection chirp model. No internal admission.
#include "PolyphonicTracker.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif
namespace flysnapper::pitch {
namespace {
constexpr int samples=512,harmonics=12,columns=24,joint=144,nonlinear=12;
constexpr double pi=3.14159265358979323846,k=0.0577622650466621;
double hz(double p){return 440*std::exp((p-69)*k);}
double midi(double f){return 69+std::log(f/440)/k;}
double dot(const float*a,const float*b,int n)noexcept{
 int i=0;double out=0;
#if defined(__SSE2__) || defined(_M_X64)
 __m128 s0=_mm_setzero_ps(),s1=s0,s2=s0,s3=s0;
 for(;i+16<=n;i+=16){s0=_mm_add_ps(s0,_mm_mul_ps(_mm_loadu_ps(a+i),_mm_loadu_ps(b+i)));s1=_mm_add_ps(s1,_mm_mul_ps(_mm_loadu_ps(a+i+4),_mm_loadu_ps(b+i+4)));s2=_mm_add_ps(s2,_mm_mul_ps(_mm_loadu_ps(a+i+8),_mm_loadu_ps(b+i+8)));s3=_mm_add_ps(s3,_mm_mul_ps(_mm_loadu_ps(a+i+12),_mm_loadu_ps(b+i+12)));}
 float t[4];_mm_storeu_ps(t,_mm_add_ps(_mm_add_ps(s0,s1),_mm_add_ps(s2,s3)));for(float v:t)out+=v;
#endif
 for(;i<n;++i)out+=double(a[i])*b[i];return out;
}
bool factor(double*m,int n,int stride)noexcept{for(int r=0;r<n;++r)for(int c=0;c<=r;++c){double v=m[r*stride+c];for(int a=0;a<c;++a)v-=m[r*stride+a]*m[c*stride+a];if(r==c){if(v<=1e-18||!std::isfinite(v))return false;m[r*stride+c]=std::sqrt(v);}else m[r*stride+c]=v/m[c*stride+c];}return true;}
void backsolve(const double*m,const double*b,double*x,int n,int stride)noexcept{std::array<double,joint> y{};for(int r=0;r<n;++r){double v=b[r];for(int c=0;c<r;++c)v-=m[r*stride+c]*y[c];y[r]=v/m[r*stride+r];}for(int r=n-1;r>=0;--r){double v=y[r];for(int c=r+1;c<n;++c)v-=m[c*stride+r]*x[c];x[r]=v/m[r*stride+r];}}
struct Model{
 double frequency=0,chirp=0,stiffness=0;int count=0;bool priorValid=false;double priorScale=.03;
 std::array<float,columns*samples> basis{};
 std::array<double,columns> mean{},coefficient{},prior{};
 std::array<double,harmonics> weight{},ratio{};std::array<bool,harmonics> valid{};
 void prepare(double f,double velocity,double rate,int n,double stiff=0)noexcept{
  frequency=f;chirp=velocity;count=n;stiffness=stiff;const double center=(n-1)*.5;
  for(int h=0;h<harmonics;++h){ratio[h]=(h+1)*std::sqrt((1+stiff*(h+1)*(h+1))/(1+stiff));weight[h]=1/std::sqrt(double(h+1));valid[h]=f*ratio[h]<rate*.45;
   double sumSin=0,sumCos=0;const double omega=2*pi*f*ratio[h]/rate,curve=pi*velocity*ratio[h]/(rate*rate),start=-center;
   const double angle=omega*start+curve*start*start,step=omega+curve*(2*start+1);double re=std::cos(angle),im=std::sin(angle),sr=std::cos(step),si=std::sin(step);const double cr=std::cos(2*curve),ci=std::sin(2*curve);
   for(int a=0;a<n;++a){basis[h*samples+a]=float(valid[h]?im*weight[h]:0);basis[(h+harmonics)*samples+a]=float(valid[h]?re*weight[h]:0);sumSin+=im;sumCos+=re;const double next=re*sr-im*si;im=re*si+im*sr;re=next;const double nsr=sr*cr-si*ci;si=sr*ci+si*cr;sr=nsr;}
   mean[h]=valid[h]?sumSin/n*weight[h]:0;mean[h+harmonics]=valid[h]?sumCos/n*weight[h]:0;
   for(int a=0;a<n;++a){basis[h*samples+a]-=float(mean[h]);basis[(h+harmonics)*samples+a]-=float(mean[h+harmonics]);}
  }
 }
 double value(int n)const noexcept{double v=0;for(int c=0;c<columns;++c)v+=basis[c*samples+n]*coefficient[c];return v;}
 double level()const noexcept{double p=0;for(int h=0;h<harmonics;++h)p+=(coefficient[h]*coefficient[h]+coefficient[h+harmonics]*coefficient[h+harmonics])*weight[h]*weight[h];return std::sqrt(p*.5);}
};
}
struct PolyphonicTracker::Data{
 struct State{Voice view;int absent=0,modelCount=0;double modelFrequency=0,modelChirp=0;std::array<double,columns> modelCoefficient{};};
 struct Admission{int note=0;bool retrigger=false;};
 double rate=12000;std::array<State,6> states{};std::array<Admission,12> admissions{};int admissionCount=0;
 std::array<Model,6> models{},trials{};std::array<int,6> slots{};
 std::array<float,samples> wave{},residual{};double energy=0;
 std::array<double,joint*joint> gram{};std::array<double,joint> rhs{},solution{};
 std::array<float,nonlinear*samples> jacobian{};
 std::array<double,joint*nonlinear> u{},projected{};
 std::array<double,nonlinear*nonlinear> normal{};std::array<double,nonlinear> gradient{},step{},thetaPrior{},variance{},precision{};
 double measurementVariance=0;
 bool fit(Model* ms,int used,int n)noexcept{
  const int dims=used*columns;for(int r=0;r<dims;++r){const auto& mr=ms[r/columns];const int c=r%columns;rhs[r]=dot(mr.basis.data()+c*samples,wave.data(),n)+(mr.priorValid?n*mr.priorScale*mr.prior[c]:0);
   for(int q=0;q<=r;++q){const auto& mq=ms[q/columns];gram[r*joint+q]=dot(mr.basis.data()+c*samples,mq.basis.data()+(q%columns)*samples,n)+(r==q?n*(.002+(mr.priorValid?mr.priorScale:0)):0);}}
  if(!factor(gram.data(),dims,joint))return false;backsolve(gram.data(),rhs.data(),solution.data(),dims,joint);for(int r=0;r<dims;++r)ms[r/columns].coefficient[r%columns]=solution[r];return true;
 }
 double residualFor(const Model*ms,int used,int n,bool store)noexcept{double sum=0;for(int a=0;a<n;++a){double e=wave[a];for(int j=0;j<used;++j)e-=ms[j].value(a);if(store)residual[a]=float(e);sum+=e*e;}return sum;}
 double objective(const Model*ms,int used,int n,double error)const noexcept{double cost=error;const double span=(n-1)/rate;for(int j=0;j<used;++j){const auto& m=ms[j];for(int c=0;c<columns;++c){cost+=n*.002*m.coefficient[c]*m.coefficient[c];if(m.priorValid){const double d=m.coefficient[c]-m.prior[c];cost+=n*m.priorScale*d*d;}}
  const double a=m.frequency-thetaPrior[2*j],b=m.chirp*span-thetaPrior[2*j+1];cost+=precision[2*j]*a*a+precision[2*j+1]*b*b;}return cost;}
 bool derivatives(int used,int n)noexcept{
  const int dims=used*columns,params=used*2;const double center=(n-1)*.5,span=(n-1)/rate;
  for(int j=0;j<used;++j){const auto&m=models[j];double means[2]{};for(int a=0;a<n;++a){const double time=(a-center)/rate;double d=0;for(int h=0;h<harmonics;++h)if(m.valid[h]){const double sine=m.basis[h*samples+a]+m.mean[h],cosine=m.basis[(h+harmonics)*samples+a]+m.mean[h+harmonics];d+=m.ratio[h]*(m.coefficient[h]*cosine-m.coefficient[h+harmonics]*sine);}
    jacobian[(2*j)*samples+a]=float(2*pi*time*d);jacobian[(2*j+1)*samples+a]=float(pi*time*time/span*d);means[0]+=jacobian[(2*j)*samples+a];means[1]+=jacobian[(2*j+1)*samples+a];}
   for(int a=0;a<n;++a){jacobian[(2*j)*samples+a]-=float(means[0]/n);jacobian[(2*j+1)*samples+a]-=float(means[1]/n);}}
  for(int p=0;p<params;++p){gradient[p]=dot(jacobian.data()+p*samples,residual.data(),n);for(int c=0;c<dims;++c)u[c*nonlinear+p]=dot(models[c/columns].basis.data()+(c%columns)*samples,jacobian.data()+p*samples,n);
   for(int c=0;c<dims;++c)rhs[c]=u[c*nonlinear+p];backsolve(gram.data(),rhs.data(),solution.data(),dims,joint);for(int c=0;c<dims;++c)projected[c*nonlinear+p]=solution[c];}
  for(int p=0;p<params;++p)for(int q=0;q<=p;++q){double v=dot(jacobian.data()+p*samples,jacobian.data()+q*samples,n);for(int c=0;c<dims;++c)v-=u[c*nonlinear+p]*projected[c*nonlinear+q];normal[p*nonlinear+q]=v+(p==q?precision[p]:0);}
  for(int p=0;p<params;++p){const double theta=p%2?models[p/2].chirp*span:models[p/2].frequency;gradient[p]+=precision[p]*(thetaPrior[p]-theta);}
  if(!factor(normal.data(),params,nonlinear))return false;backsolve(normal.data(),gradient.data(),step.data(),params,nonlinear);return true;
 }
};
PolyphonicTracker::PolyphonicTracker()=default;PolyphonicTracker::~PolyphonicTracker()=default;PolyphonicTracker::PolyphonicTracker(PolyphonicTracker&&)noexcept=default;PolyphonicTracker& PolyphonicTracker::operator=(PolyphonicTracker&&)noexcept=default;
void PolyphonicTracker::prepare(double rate){if(rate<8000||rate>48000||!std::isfinite(rate))throw std::invalid_argument("analysis rate");data=std::make_unique<Data>();data->rate=rate;}
void PolyphonicTracker::reset()noexcept{if(data){data->states={};data->admissionCount=0;}}
bool PolyphonicTracker::queueAdmission(int note,bool retrigger)noexcept{if(!data||note<28||note>100||data->admissionCount==12)return false;data->admissions[data->admissionCount++]={note,retrigger};return true;}
void PolyphonicTracker::analyse(const float*input,int count,int hop,const Settings& settings,Frame& out)noexcept{
 out={};if(!data||!input||count<128||hop<1)return;auto&d=*data;const int supplied=count;count=std::min(count,512);count-=count%32;input+=supplied-count;
 double mean=0;for(int a=0;a<count;++a){d.wave[a]=std::isfinite(input[a])?input[a]:0;mean+=d.wave[a];}mean/=count;d.energy=0;for(int a=0;a<count;++a){d.wave[a]-=float(mean);d.energy+=double(d.wave[a])*d.wave[a];}out.rms=float(std::sqrt(d.energy/count));out.observationSamples=count;
 std::array<bool,6> retrigger{};for(int a=0;a<d.admissionCount;++a){const auto ad=d.admissions[a];int slot=-1;for(int j=0;j<6;++j)if(d.states[j].view.active&&(d.states[j].view.anchorNote==ad.note||std::abs(d.states[j].view.pitch-ad.note)<.65)){slot=j;break;}if(slot>=0){retrigger[slot]|=ad.retrigger;continue;}for(int j=0;j<6;++j)if(!d.states[j].view.active){slot=j;break;}if(slot>=0){auto&s=d.states[slot];s={};s.view.active=true;s.view.id=unsigned(slot);s.view.anchorNote=ad.note;s.view.frequency=float(hz(ad.note));s.view.pitch=float(ad.note);}}
 d.admissionCount=0;int used=0;const double span=(count-1)/d.rate;
 for(int j=0;j<6&&used<std::clamp(settings.polyphony,1,6);++j)if(d.states[j].view.active){auto&s=d.states[j];auto&m=d.models[used];m.priorValid=s.modelCount>0;m.priorScale=.03;double f=s.view.frequency,v=0;
  if(m.priorValid){const double dt=(hop+s.absent+(s.modelCount-count)*.5)/d.rate;f=s.modelFrequency+s.modelChirp*dt;v=s.modelChirp;for(int h=0;h<harmonics;++h){const double angle=2*pi*(h+1)*(s.modelFrequency*dt+.5*s.modelChirp*dt*dt),cs=std::cos(angle),sn=std::sin(angle),a=s.modelCoefficient[h],b=s.modelCoefficient[h+harmonics];m.prior[h]=a*cs-b*sn;m.prior[h+harmonics]=a*sn+b*cs;}}
  f=std::clamp(f,hz(27.5),hz(100.5));v=std::clamp(v,-f*k*36,f*k*36);m.prepare(f,v,d.rate,count);d.slots[used]=j;d.thetaPrior[2*used]=f;d.thetaPrior[2*used+1]=v*span;d.variance[2*used]=std::pow(f*.003,2);d.variance[2*used+1]=std::pow(f*.015,2);++used;}
 double error=d.energy;if(used){if(!d.fit(d.models.data(),used,count))return;error=d.residualFor(d.models.data(),used,count,true);d.measurementVariance=std::max({double(settings.noiseRms)*settings.noiseRms,error/count*.3,d.energy/count*.0001,1e-14});for(int p=0;p<used*2;++p)d.precision[p]=d.measurementVariance/d.variance[p];
  for(int iteration=0;iteration<2;++iteration){const double oldCost=d.objective(d.models.data(),used,count,error);if(!d.derivatives(used,count))break;bool accepted=false;
   for(int attempt=0;attempt<2&&!accepted;++attempt){const double scale=attempt?.5:1;for(int j=0;j<used;++j){const auto&m=d.models[j];auto&t=d.trials[j];t.priorValid=m.priorValid;t.priorScale=m.priorScale;t.prior=m.prior;const double f=std::clamp(m.frequency+scale*std::clamp(d.step[2*j],-m.frequency*.03,m.frequency*.03),hz(27.5),hz(100.5)),q=m.chirp*span+scale*std::clamp(d.step[2*j+1],-m.frequency*.06,m.frequency*.06);t.prepare(f,std::clamp(q/span,-f*k*36,f*k*36),d.rate,count);}
    if(!d.fit(d.trials.data(),used,count))continue;const double trialError=d.residualFor(d.trials.data(),used,count,false),cost=d.objective(d.trials.data(),used,count,trialError);if(cost<oldCost*.999999){for(int j=0;j<used;++j)d.models[j]=d.trials[j];error=d.residualFor(d.models.data(),used,count,true);accepted=true;}}
   if(!accepted){d.fit(d.models.data(),used,count);break;}
  }
 }
 out.explained=float(std::clamp(1-error/std::max(d.energy,1e-20),0.,1.));std::array<bool,6> occupied{};const double gate=std::pow(10.,settings.newNoteThresholdDb/20);const int release=int(d.rate*std::clamp(double(settings.releaseMs),5.,300.)*.001);
 for(int j=0;j<used;++j){const int slot=d.slots[j];auto&s=d.states[slot];const auto&m=d.models[j];const double level=m.level(),f=std::clamp(m.frequency+m.chirp*span*.5,hz(27.5),hz(100.5));if(!std::isfinite(f)||level<std::max(gate*.08,double(settings.noiseRms)*1.25))continue;const bool onset=s.modelCount==0;s.modelCount=count;s.modelFrequency=m.frequency;s.modelChirp=m.chirp;s.modelCoefficient=m.coefficient;s.absent=0;occupied[slot]=true;const double confidence=std::clamp(1-error/(error+level*level*count+1e-20),0.,1.);s.view={true,onset,unsigned(slot),s.view.anchorNote,onset?0:s.view.ageSamples+hop,float(f),float(midi(f)),float(level),float(confidence),0,count,retrigger[slot],confidence>.65};}
 for(int slot=0;slot<6;++slot){auto&s=d.states[slot];if(!occupied[slot]&&s.view.active){s.view.onset=false;s.view.retrigger=false;s.absent+=hop;if(s.absent>=release)s={};}out.voices[slot]=s.view;if(s.view.active)++out.count;}
}
}
