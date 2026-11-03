// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace flysnapper::pitch {
namespace {
constexpr double pi=3.14159265358979323846;
float db(float amplitude) noexcept {return 20.0f*std::log10(std::max(amplitude,0.000001f));}
float finite(float x) noexcept {return std::isfinite(x)?std::clamp(x,-16.0f,16.0f):0.0f;}
}
PitchDetector::PitchDetector(){prepare(48000.0);}
void PitchDetector::prepare(double sr) {
    rate=std::isfinite(sr)&&sr>=8000&&sr<=384000?sr:48000.0;
    decimation=std::max(1,int(std::floor(rate/12000.0)));
    analysisRate=rate/decimation;
    lowpass=float(1.0-std::exp(-2.0*pi*(analysisRate*.42)/rate));
    dcPole=float(std::exp(-2.0*pi*12.0/rate));
    innovationCoefficient=float(1-std::exp(-1.0/(analysisRate*.0015)));
    attackCoefficient=float(1.0-std::exp(-1.0/(rate*.0006)));
    releaseCoefficient=float(1.0-std::exp(-1.0/(rate*.002)));
    fastCoefficient=float(1.0-std::exp(-1.0/(rate*.001)));slowCoefficient=float(1.0-std::exp(-1.0/(rate*.025)));
    for(int i=0;i<fftSize;++i) {
        int source=i,target=0;
        for(int bit=0;bit<11;++bit){target=(target<<1)|(source&1);source>>=1;}
        bitReverse[size_t(i)]=target;
    }
    for(int i=0;i<fftSize/2;++i)twiddle[size_t(i)]={float(std::cos(-2*pi*i/fftSize)),float(std::sin(-2*pi*i/fftSize))};
    for(int i=0;i<windowSize;++i)window[size_t(i)]=float(.5-.5*std::cos(2*pi*i/(windowSize-1)));
    for(int n=0;n<noteCount;++n)frequency[size_t(n)]=float(440.0*std::pow(2.0,(lowNote+n-69)/12.0));
    monoEstimator.prepare(analysisRate,lowNote,highNote);
    if(preparedPursuitRate!=analysisRate){pursuit.prepare(analysisRate,512);preparedPursuitRate=analysisRate;}
    Output ignored;reset(ignored);
    configure(config);
}
void PitchDetector::configure(const Settings& settings) noexcept {
    auto next=settings;
    next.thresholdDb=std::isfinite(settings.thresholdDb)?std::clamp(settings.thresholdDb,-90.0f,-12.0f):-55.0f;
    next.sensitivity=std::isfinite(settings.sensitivity)?std::clamp(settings.sensitivity,0.0f,1.0f):.65f;
    next.releaseMs=std::isfinite(settings.releaseMs)?std::clamp(settings.releaseMs,5.0f,300.0f):35.0f;
    next.polyphony=std::clamp(settings.polyphony,1,6);
    if(configured&&(next.enabled!=config.enabled||next.polyphonic!=config.polyphonic||next.polyphony!=config.polyphony))needsReset=true;
    config=next;configured=true;
    gate=std::pow(10.0f,config.thresholdDb/20.0f);
    releaseSamples=std::max(hopSize,int(analysisRate*config.releaseMs*.001));
}
void PitchDetector::emit(bool on,int note,int velocity,int offset,Output& out) noexcept {
    if(out.size>=out.events.size()){out.overflow=true;needsReset=true;return;}
    out.events[out.size++]={on,note,std::clamp(velocity,0,127),offset};
}
void PitchDetector::reset(Output& out,int offset) noexcept {
    for(int i=0;i<noteCount;++i)if(voices[size_t(i)].active)emit(false,lowNote+i,0,offset,out);
    voices={};confirmations={};scores={};strength={};previousStrength={};retriggerBaseline={};
    attackCooldown=attackRemaining=0;previousAttackRatio=0;innovationEnvelope=previousInnovationRatio=0;phaseAttack=false;
    ring={};writeIndex=filled=sinceAnalysis=signalSamples=silentSamples=decimationPhase=0;
    dcX=dcY=low1=low2=low3=low4=envelope=fastEnvelope=slowEnvelope=0;
    currentPitch=previousPitch=-1;view={};needsReset=false;
}
void PitchDetector::fft(bool inverse) noexcept {
    for(int i=0;i<fftSize;++i){int j=bitReverse[size_t(i)];if(j>i)std::swap(transform[size_t(i)],transform[size_t(j)]);}
    for(int length=2;length<=fftSize;length<<=1) {
        int half=length/2,stride=fftSize/length;
        for(int start=0;start<fftSize;start+=length)for(int j=0;j<half;++j) {
            auto w=twiddle[size_t(j*stride)];if(inverse)w.imag=-w.imag;
            auto b=transform[size_t(start+j+half)],a=transform[size_t(start+j)];
            float br=b.real*w.real-b.imag*w.imag,bi=b.real*w.imag+b.imag*w.real;
            transform[size_t(start+j)]={a.real+br,a.imag+bi};transform[size_t(start+j+half)]={a.real-br,a.imag-bi};
        }
    }
    if(inverse)for(auto& v:transform){v.real/=fftSize;v.imag/=fftSize;}
}
void PitchDetector::process(const float* mono,int samples,Output& out) noexcept {
    if(needsReset)reset(out,0);
    if(!config.enabled||samples<=0||mono==nullptr)return;
    for(int i=0;i<samples;++i) {
        float x=finite(mono[i]);
        float d=x-dcX+dcPole*dcY;dcX=x;dcY=d;
        float amplitude=std::abs(d);
        envelope+=(amplitude>envelope?attackCoefficient:releaseCoefficient)*(amplitude-envelope);
        fastEnvelope+=fastCoefficient*(amplitude-fastEnvelope);
        slowEnvelope+=slowCoefficient*(amplitude-slowEnvelope);
        low1+=lowpass*(d-low1);low2+=lowpass*(low1-low2);low3+=lowpass*(low2-low3);low4+=lowpass*(low3-low4);
        if(++decimationPhase<decimation)continue;
        decimationPhase=0;
        if(attackCooldown>0)--attackCooldown;
        if(attackRemaining>0)--attackRemaining;
        float attackRatio=fastEnvelope/std::max(slowEnvelope,gate);
        if(attackRatio>1.9f&&previousAttackRatio<=1.9f&&attackCooldown==0&&signalSamples>analysisRate*.060) {
            attackRemaining=int(analysisRate*.045);attackCooldown=int(analysisRate*.075);phaseAttack=false;
            for(int n=0;n<noteCount;++n)retriggerBaseline[size_t(n)]=voices[size_t(n)].strength;
        }
        previousAttackRatio=attackRatio;
        if(!config.polyphonic&&filled>64) {
            int active=-1;for(int n=0;n<noteCount;++n)if(voices[size_t(n)].active){active=n;break;}
            if(active>=0) {
                float lag=float(analysisRate)/frequency[size_t(active)];int whole=int(lag);float fraction=lag-whole;
                float predicted=ring[size_t((writeIndex-whole+windowSize)%windowSize)]*(1-fraction)
                    +ring[size_t((writeIndex-whole-1+windowSize)%windowSize)]*fraction;
                innovationEnvelope+=innovationCoefficient*(std::abs(low4-predicted)-innovationEnvelope);
                float ratio=innovationEnvelope/std::max(envelope,gate);
                if(ratio>.5f&&previousInnovationRatio<=.5f&&attackCooldown==0&&voices[size_t(active)].age>analysisRate*.07&&fastEnvelope>slowEnvelope*.8f) {
                    attackRemaining=int(analysisRate*.035);attackCooldown=int(analysisRate*.075);phaseAttack=true;
                    for(int n=0;n<noteCount;++n)retriggerBaseline[size_t(n)]=voices[size_t(n)].strength;
                }
                previousInnovationRatio=ratio;
            } else {innovationEnvelope=0;previousInnovationRatio=0;}
        }
        ring[size_t(writeIndex)]=low4;writeIndex=(writeIndex+1)%windowSize;filled=std::min(filled+1,windowSize);
        if(envelope>gate){++signalSamples;silentSamples=0;}else{++silentSamples;if(silentSamples>releaseSamples)signalSamples=0;}
        if(++sinceAnalysis<hopSize)continue;
        sinceAnalysis=0;analyse(i,out);
    }
}
void PitchDetector::analyse(int offset,Output& out) noexcept {
    view.levelDb=db(envelope);view.inputPresent=envelope>gate;
    strength.fill(0);scores.fill(0);currentPitch=-1;
    if(envelope>gate&&silentSamples==0&&filled>=64) {
        int count=std::min(filled,std::max(64,signalSamples+32));
        if(config.polyphonic)analysePoly(std::min(count,512),offset,out);
        else analyseMono(count,offset,out);
    }
    updateVoices(offset,out);
}
void PitchDetector::analyseMono(int count,int,Output&) noexcept {
    count=std::min(count,512);
    std::array<float,512> frame{};
    for(int i=0;i<count;++i)frame[size_t(i)]=ring[size_t((writeIndex-count+i+windowSize)%windowSize)];
    const float threshold=std::clamp(.08f+(config.sensitivity-.65f)*.08f,.035f,.11f);
    const auto estimate=monoEstimator.estimate(frame.data(),count,threshold,true);
    if(!estimate.valid)return;
    int note=estimate.note;currentPitch=estimate.pitch;
    // Hold a semitone through moderate vibrato; a confirmed new pitch outside
    // this band still ends the old note before starting the new one.
    for(int n=0;n<noteCount;++n)if(voices[size_t(n)].active&&std::abs(currentPitch-(n+lowNote))<.62f)note=n+lowNote;
    strength[size_t(note-lowNote)]=envelope;
    scores[size_t(note-lowNote)]=estimate.confidence;
    view.confidence=estimate.confidence;view.strongestNote=note;
}
void PitchDetector::analysePoly(int count,int,Output&) noexcept {
    if(count<512)return;
    std::array<float,512> frame{};
    for(int i=0;i<512;++i)frame[size_t(i)]=ring[size_t((writeIndex-512+i+windowSize)%windowSize)];
    // A low spectral crest rejects broad-band unpitched input. This check is
    // independent of the harmonic model: fitting many free partials alone can
    // otherwise explain noise with a collection of spurious low pitches.
    transform.fill({});
    for(int i=0;i<512;++i)transform[size_t(i)].real=frame[size_t(i)]*float(.5-.5*std::cos(2*pi*i/511));
    fft();
    float maximum=0,total=0,logarithms=0;int bins=0;
    const int first=std::max(1,int(50.0*fftSize/analysisRate)),last=std::min(fftSize/2,int(4500.0*fftSize/analysisRate));
    for(int k=first;k<=last;++k) {
        auto c=transform[size_t(k)];float power=c.real*c.real+c.imag*c.imag;
        maximum=std::max(maximum,power);total+=power;logarithms+=std::log(std::max(power,1e-20f));++bins;
    }
    const float flatness=std::exp(logarithms/std::max(bins,1))/(total/std::max(bins,1)+1e-20f);
    const float crest=maximum/(total/std::max(bins,1)+1e-20f);
    if(flatness>.12f||crest<6.0f)return;
    pursuit.analyse(frame.data(),512,strength,scores,config.polyphony,config.sensitivity);
    view.confidence=0;view.strongestNote=-1;
    float strongest=0;
    for(int n=0;n<noteCount;++n) {
        if(strength[size_t(n)]<gate*.8f){strength[size_t(n)]=0;scores[size_t(n)]=0;}
        if(strength[size_t(n)]>strongest){strongest=strength[size_t(n)];view.strongestNote=n+lowNote;}
        view.confidence=std::max(view.confidence,scores[size_t(n)]);
    }
}
void PitchDetector::updateVoices(int offset,Output& out) noexcept {
    int chosen=-1;
    for(int n=0;n<noteCount;++n) {
        if(strength[size_t(n)]>0) {
            if(!config.polyphonic&&previousPitch>=0&&currentPitch>=0&&std::abs(currentPitch-previousPitch)>.22f)confirmations[size_t(n)]=1;
            else ++confirmations[size_t(n)];
        }else confirmations[size_t(n)]=0;
        if(confirmations[size_t(n)]>=(scores[size_t(n)]>.97f?2:3)
            &&(config.polyphonic||(signalSamples>=analysisRate/frequency[size_t(n)]*2.25&&std::abs(currentPitch-(n+lowNote))<.40f))&&(chosen<0||strength[size_t(n)]>strength[size_t(chosen)]))chosen=n;
    }
    // In mono, a confirmed change is legato: old note off precedes new note on.
    if(!config.polyphonic&&chosen>=0)for(int n=0;n<noteCount;++n)if(n!=chosen&&voices[size_t(n)].active){emit(false,n+lowNote,0,offset,out);voices[size_t(n)]={};}
    for(int n=0;n<noteCount;++n) {
        auto& voice=voices[size_t(n)];float detected=strength[size_t(n)];
        if(detected>0) {
            voice.absent=0;
            if(voice.active&&attackRemaining>0&&voice.age>analysisRate*.07&&((phaseAttack&&fastEnvelope>slowEnvelope*.8f)||(!phaseAttack&&detected>retriggerBaseline[size_t(n)]*1.35f))) {
                emit(false,n+lowNote,0,offset,out);voice.active=false;
                retriggerBaseline[size_t(n)]=std::numeric_limits<float>::infinity();
            }
            if(!voice.active&&confirmations[size_t(n)]>=(scores[size_t(n)]>.97f?2:3)&&fastEnvelope>slowEnvelope*.35f
                &&(config.polyphonic||(signalSamples>=analysisRate/frequency[size_t(n)]*2.25&&std::abs(currentPitch-(n+lowNote))<.40f))) {
                if(config.polyphonic) {
                    int held=0,weakest=-1;
                    for(int j=0;j<noteCount;++j)if(voices[size_t(j)].active){++held;
                        if(strength[size_t(j)]==0&&(weakest<0||voices[size_t(j)].strength<voices[size_t(weakest)].strength))weakest=j;}
                    if(held>=config.polyphony) {
                        if(weakest<0)continue;
                        emit(false,weakest+lowNote,0,offset,out);voices[size_t(weakest)]={};
                    }
                }
                // Root-sum amplitude tracks independent polyphonic velocities.
                int velocity=std::clamp(int(std::lround(127.0f*std::pow(std::min(detected*2.0f,1.0f),.45f))),1,127);
                emit(true,n+lowNote,velocity,offset,out);voice.active=true;voice.age=0;
            }
            voice.strength=detected;
        } else if(voice.active) {
            voice.absent+=hopSize;
            if(voice.absent>=releaseSamples){emit(false,n+lowNote,0,offset,out);voice={};}
        }
        if(voice.active)voice.age+=hopSize;
        previousStrength[size_t(n)]=detected;
    }
    previousPitch=currentPitch;
    view.activeNotes=0;view.activePitchClasses=0;
    for(int n=0;n<noteCount;++n)if(voices[size_t(n)].active){++view.activeNotes;view.activePitchClasses=uint16_t(view.activePitchClasses|(1u<<((n+lowNote)%12)));}
    if(view.activeNotes==0&&envelope<=gate){view.confidence=0;view.strongestNote=-1;}
}
}
