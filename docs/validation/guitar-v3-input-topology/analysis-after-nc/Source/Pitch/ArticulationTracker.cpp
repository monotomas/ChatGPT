// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ArticulationTracker.h"
#include <algorithm>
#include <cmath>

namespace flysnapper::pitch {
void ArticulationTracker::prepare(double sampleRate) noexcept {
    rate=std::isfinite(sampleRate)&&sampleRate>=1000?sampleRate:12000;
    reset();
}
void ArticulationTracker::reset() noexcept {
    anchor=-1;flat=0;reference=origin=previous=-1;
    previousObservationSamples=movementObservationSamples=0;
    previousSample=lastNearReference=movementStart=0;peakSpeed=0;moving=false;
}
void ArticulationTracker::begin(int note,float pitch,int64_t sample) noexcept {
    reset();
    if(note<0||note>127||!std::isfinite(pitch))return;
    anchor=note;reference=origin=previous=pitch;
    previousSample=lastNearReference=movementStart=sample;
}
ArticulationTracker::Decision ArticulationTracker::observe(const Observation& input) noexcept {
    Decision result;result.note=anchor;result.pitch=input.pitch;
    if(anchor<0||!std::isfinite(input.pitch)||!std::isfinite(input.confidence)
        ||input.confidence<.85f||input.sample<=previousSample)return result;
    const double elapsed=(input.sample-previousSample)/rate;
    if(elapsed>.080){
        // A long gap supplies no evidence about the path between pitches.
        // Voice release/reacquisition belongs to the caller.
        begin(anchor,input.pitch,input.sample);return result;
    }
    const float change=input.pitch-previous;
    const float speed=float(change/elapsed),absoluteSpeed=std::abs(speed);
    const float departure=input.pitch-reference;
    if(!moving){
        if(std::abs(departure)>.10f){
            moving=true;origin=reference;movementStart=lastNearReference;peakSpeed=absoluteSpeed;flat=0;
            movementObservationSamples=std::max(input.observationSamples,previousObservationSamples);
        }else{
            reference+=.10f*departure;lastNearReference=input.sample;
        }
    }
    if(moving){
        const float distance=input.pitch-origin;
        peakSpeed=std::max(peakSpeed,absoluteSpeed);
        // A returned vibrato excursion does not create a new oscillator.
        if(std::abs(distance)<.07f){moving=false;reference=input.pitch;lastNearReference=input.sample;flat=0;}
        else{
            flat=absoluteSpeed<15.f&&std::abs(change)<.08f?flat+1:0;
            const double duration=std::max(1.0/rate,(input.sample-movementStart)/rate);
            movementObservationSamples=std::max(movementObservationSamples,input.observationSamples);
            const double observation=std::clamp(movementObservationSamples/rate,.005,.085);
            const float semitones=float(std::lround(distance));
            const bool fretDistance=std::abs(semitones)>=1&&std::abs(distance-semitones)<.14f;
            // A window smears a physical step over its own duration. Compare
            // the observed change against that bandwidth rather than using
            // one fixed pitch-velocity threshold for every guitar register.
            const double sharpness=peakSpeed*observation/std::max(.5f,std::abs(distance));
            const bool rapidStep=duration<=std::clamp(observation*.95+2*elapsed,.012,.060)
                &&sharpness>=1.45;
            const bool independentExcitation=std::isfinite(input.excitation)&&input.excitation>=.75f
                &&duration<=std::max(.065,observation*2.5);
            if(fretDistance&&flat>=1&&input.confidence>=.94f&&(rapidStep||independentExcitation)){
                result.newNote=true;result.note=std::clamp(int(std::lround(input.pitch)),0,127);
                result.confidence=float(std::clamp(std::max(sharpness/2.,double(input.excitation)),0.,1.));
            }else if(flat>=5){
                // A smooth bend may land on a fret pitch and remain there.
                // Its stable endpoint becomes the reference for a later
                // hammer/pull, while the sounding MIDI anchor stays unchanged.
                reference=input.pitch;lastNearReference=input.sample;moving=false;flat=0;
            }
        }
    }
    previous=input.pitch;previousSample=input.sample;
    previousObservationSamples=input.observationSamples;
    result.moving=moving;
    return result;
}

void PartialPhaseTracker::prepare(double sampleRate) noexcept {
    rate=std::isfinite(sampleRate)&&sampleRate>=1000?sampleRate:12000;
    reset();
}
void PartialPhaseTracker::reset() noexcept {
    previous={};trend={};departure={};valid={};previousPower={};powerDeparture={};previousSample=excitationSample=0;
    previousPitch=-1;heldExcitation=0;powerValid=false;
}
float PartialPhaseTracker::observe(const float* wave,int samples,float frequency,int64_t sample) noexcept {
    if(!wave||!std::isfinite(frequency)||frequency<=0||samples<32||sample<=previousSample)return 0;
    constexpr double pi=3.14159265358979323846;
    const double period=rate/frequency;
    const int count=std::clamp(int(std::lround(period*2)),32,std::min(samples,1024));
    if(count<period*1.8)return 0;
    wave+=samples-count;
    // A small joint least-squares harmonic fit avoids rectangular-window
    // leakage between partials when two periods are not an integer sample
    // count. All dimensions and work are bounded.
    constexpr int dimension=9;
    double system[dimension][dimension+1]{};
    const double omega=2*pi/period,stepC=std::cos(omega),stepS=std::sin(omega);
    double c=std::cos(-omega*(count-1)*.5),s=std::sin(-omega*(count-1)*.5);
    for(int i=0;i<count;++i){
        double basis[dimension]{1};double hc=c,hs=s;
        for(int h=0;h<4;++h){
            basis[1+2*h]=hc;basis[2+2*h]=hs;
            const double next=hc*c-hs*s;hs=hs*c+hc*s;hc=next;
        }
        for(int a=0;a<dimension;++a){
            system[a][dimension]+=basis[a]*wave[i];
            for(int b=0;b<=a;++b)system[a][b]+=basis[a]*basis[b];
        }
        const double next=c*stepC-s*stepS;s=s*stepC+c*stepS;c=next;
    }
    for(int a=0;a<dimension;++a)for(int b=a+1;b<dimension;++b)system[a][b]=system[b][a];
    for(int a=0;a<dimension;++a){
        int pivot=a;for(int b=a+1;b<dimension;++b)if(std::abs(system[b][a])>std::abs(system[pivot][a]))pivot=b;
        if(std::abs(system[pivot][a])<1e-8)return 0;
        if(pivot!=a)for(int b=a;b<=dimension;++b)std::swap(system[a][b],system[pivot][b]);
        const double divisor=system[a][a];for(int b=a;b<=dimension;++b)system[a][b]/=divisor;
        for(int row=0;row<dimension;++row)if(row!=a){const double factor=system[row][a];
            for(int b=a;b<=dimension;++b)system[row][b]-=factor*system[a][b];}
    }
    std::array<double,4> phase{},power{};double maximum=0;
    for(int h=0;h<4;++h){const double a=system[1+2*h][dimension],b=system[2+2*h][dimension];
        phase[size_t(h)]=std::atan2(-b,a);power[size_t(h)]=a*a+b*b;maximum=std::max(maximum,power[size_t(h)]);}
    const float pitch=float(69+12*std::log2(frequency/440.));
    const double dt=previousSample>0?(sample-previousSample)/rate:0;
    const bool stable=previousPitch>=0&&std::abs(pitch-previousPitch)<.065f;
    double largest=0;int changed=0,supported=0;
    for(int h=2;h<=4;++h){
        const size_t index=size_t(h-2);
        const bool supportedNow=h*frequency<rate*.45&&maximum>1e-16&&power[0]>maximum*.02&&power[size_t(h-1)]>maximum*.005;
        const double relative=std::remainder(phase[size_t(h-1)]-h*phase[0],2*pi);
        if(supportedNow&&valid[index]&&dt>0&&dt<.030){
            const double change=std::remainder(relative-previous[index],2*pi);
            const double innovation=std::remainder(change-trend[index]*dt,2*pi);
            departure[index]=departure[index]*std::exp(-dt/.040)+innovation;
            const double distance=std::abs(departure[index]);largest=std::max(largest,distance);
            changed+=distance>.32;++supported;
            if(stable&&distance<.20)trend[index]=.9*trend[index]+.1*change/dt;
        }else{trend[index]=0;departure[index]=0;}
        previous[index]=relative;valid[index]=supportedNow;
    }
    float excitation=0;
    if(supported>=1&&(largest>.65||changed>=2))excitation=float(std::clamp(largest/.9,0.,1.));
    double totalPower=0;for(double value:power)totalPower+=value;
    double powerChange=0;
    for(size_t h=0;h<power.size();++h){
        const double normalized=totalPower>1e-16?power[h]/totalPower:0;
        powerDeparture[h]=powerValid&&dt>0&&dt<.030
            ?powerDeparture[h]*std::exp(-dt/.025)+normalized-previousPower[h]:0;
        powerChange+=std::abs(powerDeparture[h]);previousPower[h]=normalized;
    }
    powerValid=totalPower>1e-16;
    // A fretting step can preserve phase yet abruptly withdraw the old odd
    // partials (for example a clean octave hammer). Slow pickup/timbre changes
    // are tracked by the decaying profile rather than labelled an attack.
    if(powerChange>.32)excitation=std::max(excitation,float(std::clamp(powerChange/.6,0.,1.)));
    if(excitation>heldExcitation){heldExcitation=excitation;excitationSample=sample;}
    if(sample-excitationSample>rate*.040)heldExcitation*=float(std::exp(-std::max(dt,0.)/.010));
    previousPitch=pitch;previousSample=sample;
    return heldExcitation;
}
}
