// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Pitch/HarmonicContinuity.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
constexpr double pi=3.14159265358979323846,rate=12000;
using Evidence=flysnapper::pitch::HarmonicContinuity::Evidence;
struct Row {int sample=0;Evidence evidence;};
struct Random {std::uint32_t value=1234567;double next(){value=value*1664525+1013904223;return (double(value)/4294967296.-.5)*2;}};
double tone(double phase,double stiffness=0,double time=0){double x=0;for(int h=1;h<=8;++h)x+=.1*std::sin(h*phase+(stiffness?2*pi*110*h*(std::sqrt(1+stiffness*h*h)-1)*time:0)+h*.17)/std::pow(double(h),1.3);return x;}
std::vector<Row> run(const std::function<double(int)>& signal,const std::function<double(int)>& pitch,int length=12000,
                     const std::function<bool(int)>& frozen={}){
    flysnapper::pitch::HarmonicContinuity observer;observer.prepare(rate);
    std::array<float,1024> ring{},frame{};int write=0,filled=0;std::vector<Row> rows;
    for(int i=0;i<length;++i){ring[size_t(write)]=float(signal(i));write=(write+1)%1024;filled=std::min(1024,filled+1);
        if(i%32||filled<64)continue;
        for(int k=0;k<filled;++k)frame[size_t(k)]=ring[size_t((write-filled+k+1024)%1024)];
        observer.freezeReference(frozen&&frozen(i));
        rows.push_back({i,observer.observe(frame.data(),filled,float(pitch(i)),i)});
    }return rows;
}
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
bool energyEvidence(const Evidence& e){return e.coherentEnergyRisePartials>=3&&e.foregroundNovelty>.25f&&e.noveltyFraction>.65f;}
}
int main(){try{
    const auto frequency=[](int){return 110.;};
    const auto stable=run([](int i){return tone(2*pi*110*i/rate);},frequency);
    for(const auto& row:stable)if(row.sample>1200){check(row.evidence.phaseSupport==8,"Stable source lost independent partials");check(row.evidence.phaseContinuity>.9999,"Stable absolute phase advance failed");check(row.evidence.departurePartials==0,"Stable source acquired false phase departure");check(!energyEvidence(row.evidence),"Stable source acquired energy excitement");}
    std::cout<<"stable PASS\n";
    const auto stiff=run([](int i){return tone(2*pi*110*i/rate,.000055,i/rate);},frequency);
    for(const auto& row:stiff)if(row.sample>3600){check(row.evidence.phaseContinuity>.999,"Stiff string partials did not retain independent detuning");check(row.evidence.departurePartials==0,"Stiff string was mistaken for phase reset");}
    std::cout<<"stiff PASS\n";
    for(double gain:{.1,10.}){const auto scaled=run([&](int i){return gain*tone(2*pi*110*i/rate);},frequency);
        for(size_t i=0;i<scaled.size();++i)check(std::abs(scaled[i].evidence.phaseContinuity-stable[i].evidence.phaseContinuity)<.0001,"Phase continuity changed with common gain");}
    std::cout<<"gain PASS\n";
    const auto single=run([](int i){const double t=i/rate;return tone(2*pi*110*t)+(t>=.3&&t<.5?.2*std::sin(2*pi*110*t+pi*.5):0);},frequency);
    int singleEnergy=0,singleDeparture=0,minStable=8;double minimumContinuity=1;
    for(const auto& row:single)if(row.sample>=3600&&row.sample<6600){singleEnergy+=energyEvidence(row.evidence);singleDeparture=std::max(singleDeparture,row.evidence.departurePartials);minStable=std::min(minStable,row.evidence.continuingPartials);minimumContinuity=std::min(minimumContinuity,double(row.evidence.phaseContinuity));}
    std::cout<<"fundamental_contamination energy="<<singleEnergy<<" max_departure="<<singleDeparture<<" min_continuing="<<minStable<<" min_continuity="<<minimumContinuity<<'\n';
    for(const auto& row:single)if(energyEvidence(row.evidence)){const auto& e=row.evidence;std::cout<<"false_energy i"<<row.sample<<" gain"<<e.commonEnergyGainDb<<" support"<<e.coherentEnergyRisePartials<<" novelty"<<e.foregroundNovelty<<" fraction"<<e.noveltyFraction<<" addedResidual/oldForeground"<<e.residualDeltaPower/e.baselineForegroundPower<<'\n';
        for(int h=0;h<8;++h){const auto& p=e.partials[size_t(h)];std::cout<<" h"<<h+1<<" ratio"<<p.power/p.baselinePower<<" phase"<<p.phaseInnovation<<" dep"<<p.phaseDeparture<<" trusted"<<p.phaseTrusted<<" rise"<<p.energyRising<<'\n';}}
    check(singleEnergy==0,"One contaminated fundamental produced independent harmonic energy proof");
    for(const auto& row:single)if(row.sample>=4080&&row.sample<5760)
        check(row.evidence.continuingPartials>=6,"Steady single-band contamination invalidated upper-partial continuity");
    Random random;const auto noisy=run([&](int i){const double t=i/rate;return tone(2*pi*110*t)+(t>=.3&&t<.34?random.next()*.15:0);},frequency);
    int noiseEnergy=0;double maxNovelty=0;
    for(const auto& row:noisy)if(row.sample>=3600&&row.sample<4800){noiseEnergy+=energyEvidence(row.evidence);if(row.evidence.energyRisePartials>=3&&row.evidence.foregroundNovelty>.25)maxNovelty=std::max(maxNovelty,double(row.evidence.noveltyFraction));}
    std::cout<<"broadband energy="<<noiseEnergy<<" max_novelty_fraction="<<maxNovelty<<'\n';
    check(noiseEnergy==0,"A broadband residual burst produced independent target-energy proof");
    const auto phaseReset=run([](int i){const double t=i/rate;return tone(2*pi*110*t+(t>=.3?.5:0));},frequency);
    int resetSupport=0;double bestReset=0,bestCoherence=0;
    for(const auto& row:phaseReset)if(row.sample>=3600&&row.sample<4320){if(row.evidence.departurePartials>resetSupport){resetSupport=row.evidence.departurePartials;bestReset=row.evidence.commonResetRadians;bestCoherence=row.evidence.resetCoherence;}}
    std::cout<<"coherent_phase_reset max_departure="<<resetSupport<<" reset="<<bestReset<<" coherence="<<bestCoherence<<'\n';
    check(resetSupport>=3,"True common phase reset lacked independent partial departure");
    const auto repick=run([](int i){const double t=i/rate,gain=1+.8*std::clamp((t-.3)/.001,0.,1.);return gain*tone(2*pi*110*t);},frequency);
    int repickProof=0;for(const auto& row:repick)if(row.sample>=3600&&row.sample<4320)repickProof+=energyEvidence(row.evidence);
    std::cout<<"phase_coherent_repick energy="<<repickProof<<'\n';check(repickProof>0,"Phase-coherent harmonic-energy rise lacked independent proof");
    const double slope=std::log(2.)*2/12/.25;
    const auto chirp=run([&](int i){const double t=i/rate;return tone(2*pi*110*(std::exp(slope*t)-1)/slope);},[&](int i){return 110*std::exp(slope*i/rate);},6000);
    double minChirp=1;int maxChirpReset=0,chirpEnergy=0;
    for(const auto& row:chirp)if(row.sample>1200){minChirp=std::min(minChirp,double(row.evidence.phaseContinuity));maxChirpReset=std::max(maxChirpReset,row.evidence.departurePartials);chirpEnergy+=energyEvidence(row.evidence);}
    std::cout<<"smooth_chirp min_continuity="<<minChirp<<" max_departure="<<maxChirpReset<<" energy="<<chirpEnergy<<'\n';
    check(minChirp>.9&&chirpEnergy==0,"Continuous pitch motion produced false energy articulation");
    std::cout<<"HarmonicContinuity mechanism PASS (research only, no recorded mixture acceptance).\n";
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
