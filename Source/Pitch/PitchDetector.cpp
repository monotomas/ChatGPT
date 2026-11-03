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
// Validate a frequency hypothesis against the supplied past-only observation.
// A local log-power fit checks peak-centre agreement within 1.5%; a broad
// Rayleigh main lobe alone must not validate a nearby body resonance.
template<size_t N>
std::array<double,2> projectPhase(const std::array<float,N>& wave,int start,int count,double firstTime,double period,int harmonic=1) noexcept {
    const double omega=2*pi*harmonic/period;
    double re=std::cos(omega*firstTime),im=-std::sin(omega*firstTime);
    const double stepRe=std::cos(omega),stepIm=-std::sin(omega);
    std::array<double,2> result{};
    for(int i=0;i<count;++i){
        const double x=wave[size_t((start+i+int(N)*2)%int(N))];result[0]+=x*re;result[1]+=x*im;
        const double next=re*stepRe-im*stepIm;im=re*stepIm+im*stepRe;re=next;
    }
    return result;
}

struct MonoSeriesEvidence {uint16_t mask=0;int count=0;bool root=false;};
MonoSeriesEvidence measureMonoSeries(const float* wave,int count,double frequency,double rate) noexcept {
    MonoSeriesEvidence evidence;
    if(count<128||count>1024||frequency<=0||frequency>=rate*.45)return evidence;
    std::array<float,1024> windowed{};double mean=0;
    for(int i=0;i<count;++i)mean+=wave[i];
    mean/=count;
    const double windowAngle=2*pi/(count-1),stepC=std::cos(windowAngle),stepS=std::sin(windowAngle);double c=1,s=0;
    for(int i=0;i<count;++i){windowed[size_t(i)]=float((wave[i]-mean)*(.5-.5*c));const double next=c*stepC-s*stepS;s=s*stepC+c*stepS;c=next;}
    const auto power=[&](double hz) noexcept {
        if(hz<=0||hz>=rate*.45)return 0.;
        const double angle=2*pi*hz/rate,dc=std::cos(angle),ds=std::sin(angle);double c=1,s=0,re=0,im=0;
        for(int i=0;i<count;++i){re+=windowed[size_t(i)]*c;im+=windowed[size_t(i)]*s;const double next=c*dc-s*ds;s=s*dc+c*ds;c=next;}
        return re*re+im*im;
    };
    std::array<double,12> powers{};double maximum=0;
    for(int h=1;h<=12;++h){powers[size_t(h-1)]=power(frequency*h);maximum=std::max(maximum,powers[size_t(h-1)]);}
    if(maximum<1e-20)return evidence;
    for(int h=1;h<=12;++h){
        const double center=powers[size_t(h-1)];if(center<maximum*.0016)continue;
        const double hz=frequency*h,a=std::log(std::max(power(hz*.985),1e-30)),b=std::log(std::max(center,1e-30)),d=std::log(std::max(power(hz*1.015),1e-30));
        const double curvature=a-2*b+d;if(curvature>=-1e-12)continue;
        const double displacement=.5*(a-d)/curvature;
        if(std::abs(displacement)>1)continue;
        evidence.mask=uint16_t(evidence.mask|(1u<<(h-1)));++evidence.count;if(h==1)evidence.root=true;
    }
    return evidence;
}
// Low-order coprime partials can establish a genuinely missing fundamental.
// Preserve that strict candidate instead of preferring a stronger overtone.
bool hasPrimitiveLowSeries(uint16_t mask) noexcept {
    for(int a=2;a<=3;++a)if(mask&(1u<<(a-1)))for(int b=a+1;b<=12;++b)
        if((mask&(1u<<(b-1)))&&std::gcd(a,b)==1)return true;
    return false;
}
}
PitchDetector::PitchDetector(){prepare(48000.0);}
void PitchDetector::prepare(double sr) {
    rate=std::isfinite(sr)&&sr>=8000&&sr<=384000?sr:48000.0;
    // A fixed analysis rate keeps the observation interval and harmonic
    // resolution equal across 44.1/48/96kHz hosts. Interpolation below uses
    // only the two filtered samples already received, never future audio.
    analysisRate=std::min(rate,12000.0);analysisStep=analysisRate/rate;
    lowpass=float(1.0-std::exp(-2.0*pi*(analysisRate*.42)/rate));
    dcPole=float(std::exp(-2.0*pi*12.0/rate));
    innovationCoefficient=float(1-std::exp(-1.0/(analysisRate*.0015)));
    onsetCoefficient=float(1-std::exp(-1.0/(rate*.006)));
    spectralUpdate=float(1-std::exp(-hopSize/(analysisRate*.040)));
    predictionFastCoefficient=float(1-std::exp(-1/(analysisRate*.003)));
    predictionSlowCoefficient=float(1-std::exp(-1/(analysisRate*.060)));
    attackCoefficient=float(1.0-std::exp(-1.0/(rate*.0006)));
    releaseCoefficient=float(1.0-std::exp(-1.0/(rate*.002)));
    fastCoefficient=float(1.0-std::exp(-1.0/(rate*.001)));slowCoefficient=float(1.0-std::exp(-1.0/(rate*.025)));
    for(int i=0;i<fftSize;++i) {
        int source=i,target=0;
        for(int bit=0;bit<11;++bit){target=(target<<1)|(source&1);source>>=1;}
        bitReverse[size_t(i)]=target;
    }
    for(int i=0;i<fftSize/2;++i)twiddle[size_t(i)]={float(std::cos(-2*pi*i/fftSize)),float(std::sin(-2*pi*i/fftSize))};
    for(int i=0;i<512;++i)window[size_t(i)]=float(.5-.5*std::cos(2*pi*i/511));
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
    next.bendRangeSemitones=std::isfinite(settings.bendRangeSemitones)?std::clamp(settings.bendRangeSemitones,2.0f,12.0f):2.0f;
    const bool previousBend=config.monoBend&&!config.polyphonic;
    const bool nextBend=next.monoBend&&!next.polyphonic;
    if(configured&&(next.enabled!=config.enabled||next.polyphonic!=config.polyphonic||next.polyphony!=config.polyphony
        ||previousBend!=nextBend||(nextBend&&next.bendRangeSemitones!=config.bendRangeSemitones)))needsReset=true;
    config=next;configured=true;
    gate=std::pow(10.0f,config.thresholdDb/20.0f);
    releaseSamples=std::max(hopSize,int(analysisRate*config.releaseMs*.001));
}
void PitchDetector::emit(bool on,int note,int velocity,int offset,Output& out) noexcept {
    if(out.size>=out.events.size()){out.overflow=true;needsReset=true;return;}
    out.events[out.size++]={on,note,std::clamp(velocity,0,127),offset};
    if(!on&&lastPitchAnchor==note){lastPitchAnchor=-1;lastSentPitch=-1;}
}
void PitchDetector::emitPitch(int note,float pitch,int offset,Output& out) noexcept {
    if(!std::isfinite(pitch))return;
    if(lastPitchAnchor==note&&std::abs(pitch-lastSentPitch)<.003f)return;
    if(out.size>=out.events.size()){out.overflow=true;needsReset=true;return;}
    out.events[out.size++]={false,note,0,offset,true,pitch};lastPitchAnchor=note;lastSentPitch=pitch;
}
void PitchDetector::reset(Output& out,int offset) noexcept {
    for(int i=0;i<noteCount;++i)if(voices[size_t(i)].active)emit(false,lowNote+i,0,offset,out);
    voices={};confirmations={};scores={};strength={};retriggerBaseline={};
    attackCooldown=attackRemaining=0;previousAttackRatio=0;onsetEnvelope=0;innovationEnvelope=previousInnovationRatio=0;phaseAttack=false;phaseAttackAge=pitchHistoryCount=pitchHistoryWrite=0;pitchHistory={};pitchHistoryRange=0;
    ring={};writeIndex=filled=sinceAnalysis=signalSamples=silentSamples=0;analysisPhase=0;previousFiltered=0;
    dcX=dcY=low1=low2=low3=low4=envelope=fastEnvelope=slowEnvelope=0;
    currentPitch=previousPitch=-1;measuredPeriod=0;measuredPeriodNote=-1;lastSentPitch=-1;lastPitchAnchor=-1;phaseAttackOriginPitch=-1;monoTrackedOnly=monoPhysicalStable=false;spectralBaseline={};polyAdmission=0;polyAnchor=-1;pendingPolyRepick=-1;pendingPolyRepickAge=0;pendingPhasePeriod=0;pendingPhaseCoefficients={};independentNovelty={};
    predictionValid=false;predictionIndex=hopSize;predictionAge=predictionCooldown=0;predictionFast=predictionSlow=predictionObserved=predictionExpected=0;polyPrediction={};
    view={};needsReset=false;
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
        // Short amplitude smoothing follows the measured period, avoiding
        // false attacks at each high-crest bass waveform maximum. Input gate
        // and falling-input measurements keep their independent fast paths.
        onsetEnvelope+=onsetCoefficient*(amplitude-onsetEnvelope);
        low1+=lowpass*(d-low1);low2+=lowpass*(low1-low2);low3+=lowpass*(low2-low3);low4+=lowpass*(low3-low4);
        const float priorFiltered=previousFiltered;previousFiltered=low4;
        analysisPhase+=analysisStep;
        if(analysisPhase<1.0)continue;
        analysisPhase-=1.0;
        const float fraction=float(1.0-analysisPhase/analysisStep);
        const float analysisSample=priorFiltered+fraction*(low4-priorFiltered);
        if(attackCooldown>0)--attackCooldown;
        if(pendingPolyRepick>=0){
            pendingPolyRepickAge=std::min(pendingPolyRepickAge+1,windowSize*16);
            if(pendingPolyRepickAge>analysisRate*.120)pendingPolyRepick=-1;
        }
        if(polyAdmission>0)--polyAdmission;
        if(predictionCooldown>0)--predictionCooldown;
        if(config.polyphonic&&predictionValid&&predictionIndex<hopSize) {
            const float predicted=polyPrediction[size_t(predictionIndex++)];
            const float error=analysisSample-predicted;
            // A withdrawal of acoustic energy is a release, even when it
            // surprises the oscillator model. Do not reopen admission from it.
            predictionObserved+=predictionFastCoefficient*(analysisSample*analysisSample-predictionObserved);
            predictionExpected+=predictionFastCoefficient*(predicted*predicted-predictionExpected);
            predictionFast+=predictionFastCoefficient*(error*error-predictionFast);
            predictionSlow+=predictionSlowCoefficient*(error*error-predictionSlow);
            predictionAge=std::min(predictionAge+1,windowSize*16);
            if(predictionAge>analysisRate*.040&&predictionCooldown==0&&predictionFast>predictionSlow*4
                &&std::sqrt(predictionFast)-std::sqrt(predictionSlow)>std::max(slowEnvelope,gate)*.10f
                &&predictionObserved>=predictionExpected*.95f&&fastEnvelope>slowEnvelope*.7f) {
                polyAdmission=int(analysisRate*.120);predictionCooldown=int(analysisRate*.075);
            }
        }
        if(attackRemaining>0){--attackRemaining;if(phaseAttack)++phaseAttackAge;}
        float attackRatio=(config.polyphonic?fastEnvelope:onsetEnvelope)/std::max(slowEnvelope,gate);
        if(attackRatio>1.9f&&previousAttackRatio<=1.9f&&attackCooldown==0&&signalSamples>analysisRate*.060) {
            attackRemaining=int(analysisRate*.045);attackCooldown=int(analysisRate*.075);phaseAttack=false;
            for(int n=0;n<noteCount;++n)retriggerBaseline[size_t(n)]=voices[size_t(n)].strength;
        }
        previousAttackRatio=attackRatio;
        if(filled>64) {
            int active=-1,held=0;for(int n=0;n<noteCount;++n)if(voices[size_t(n)].active){active=n;++held;}
            if(config.polyphonic&&(held!=1||polyAnchor!=active))active=-1;
            if(active>=0) {
                float lag=measuredPeriodNote==active&&measuredPeriod>0?measuredPeriod:float(analysisRate)/frequency[size_t(active)];int whole=int(lag);float fraction=lag-whole;
                float predicted=ring[size_t((writeIndex-whole+windowSize)%windowSize)]*(1-fraction)
                    +ring[size_t((writeIndex-whole-1+windowSize)%windowSize)]*fraction;
                innovationEnvelope+=innovationCoefficient*(std::abs(analysisSample-predicted)-innovationEnvelope);
                float ratio=innovationEnvelope/std::max(envelope,gate);
                if(ratio>.5f&&previousInnovationRatio<=.5f&&attackCooldown==0&&voices[size_t(active)].age>analysisRate*.07) {
                    if(config.polyphonic){
                        pendingPolyRepick=active;pendingPolyRepickAge=0;attackCooldown=int(analysisRate*.075);
                        pendingPhasePeriod=std::clamp(lag,2.0f,float(windowSize)/3.0f);
                        const int past=int(std::lround(3*pendingPhasePeriod)),recent=int(std::lround(2*pendingPhasePeriod)),n=int(std::lround(pendingPhasePeriod));
                        if(filled<past)pendingPolyRepick=-1;
                        else {
                            pendingPhaseCoefficients.fill(0);
                            for(int group=0;group<4;++group){const int harmonic=2*group+1;if(harmonic>=pendingPhasePeriod*.45)continue;
                                const auto older=projectPhase(ring,writeIndex-past,n,-double(past),pendingPhasePeriod,harmonic);
                                const auto newer=projectPhase(ring,writeIndex-recent,n,-double(recent),pendingPhasePeriod,harmonic);
                                pendingPhaseCoefficients[size_t(4*group)]=older[0];pendingPhaseCoefficients[size_t(4*group+1)]=older[1];
                                pendingPhaseCoefficients[size_t(4*group+2)]=newer[0];pendingPhaseCoefficients[size_t(4*group+3)]=newer[1];
                            }
                        }
                    }else{
                        attackRemaining=int(analysisRate*.060);attackCooldown=int(analysisRate*.075);phaseAttack=true;phaseAttackAge=0;
                        phaseAttackOriginPitch=currentPitch;
                        pitchHistoryCount=pitchHistoryWrite=0;confirmations.fill(0);previousPitch=-1;
                        for(int n=0;n<noteCount;++n)retriggerBaseline[size_t(n)]=voices[size_t(n)].strength;
                    }
                }
                previousInnovationRatio=ratio;
            } else {innovationEnvelope=0;previousInnovationRatio=0;}
        }
        ring[size_t(writeIndex)]=analysisSample;writeIndex=(writeIndex+1)%windowSize;filled=std::min(filled+1,windowSize);
        // Acquisition history retains the ordinary input gate. A previously
        // acquired string has a separate lower continuation gate below; its
        // long quiet decay must not make the next fresh attack inherit a
        // full window containing a gap or a DC-blocker tail.
        if(envelope>gate){if(signalSamples==0)polyAdmission=int(analysisRate*.140);signalSamples=std::min(signalSamples+1,windowSize*16);silentSamples=0;}else{silentSamples=std::min(silentSamples+1,std::max(windowSize*16,releaseSamples+windowSize));if(silentSamples>releaseSamples)signalSamples=0;}
        if(++sinceAnalysis<hopSize)continue;
        sinceAnalysis=0;analyse(i,out);
    }
}
void PitchDetector::analyse(int offset,Output& out) noexcept {
    view.levelDb=db(envelope);view.inputPresent=envelope>gate;
    strength.fill(0);scores.fill(0);currentPitch=-1;polyAnchor=-1;monoTrackedOnly=monoPhysicalStable=false;independentNovelty.fill(false);
    const float trackingGate=view.activeNotes>0?gate*.08f:gate;
    if(envelope>trackingGate&&(view.activeNotes>0||silentSamples==0)&&filled>=64) {
        int count=view.activeNotes>0?filled:std::min(filled,std::max(64,signalSamples+32));
        if(config.polyphonic)analysePoly(std::min(count,512),offset,out);
        else analyseMono(count,offset,out);
    }
    updateVoices(offset,out);
    if(config.polyphonic&&phaseAttack){phaseAttack=false;attackRemaining=0;}
}
void PitchDetector::analyseMono(int count,int,Output&) noexcept {
    count=std::min(count,windowSize);
    int held=-1;for(int n=0;n<noteCount;++n)if(voices[size_t(n)].active)held=n;
    const int freshCount=phaseAttack&&attackRemaining>0?std::min(count,phaseAttackAge):count;
    if(freshCount<32&&held<0)return;
    std::array<float,windowSize> frame{};
    for(int i=0;i<count;++i)frame[size_t(i)]=ring[size_t((writeIndex-count+i+windowSize)%windowSize)];
    const float threshold=std::clamp(.08f+(config.sensitivity-.65f)*.08f,.035f,.11f);
    const int primary=std::min(freshCount,512);
    auto estimate=monoEstimator.estimate(frame.data()+count-primary,primary,threshold,true);
    // A body transient can make a long common period pass the strict YIN test
    // before the actual source period. Consider a second, bounded candidate
    // only when the strict result lacks physical root/primitive-series support.
    // Never turn a strict-invalid observation into an early low-confidence note.
    // An already-held strict note retains its identity through timbre changes.
    if(estimate.valid&&primary>=128&&!voices[size_t(estimate.note-lowNote)].active){
        const auto secondary=monoEstimator.estimate(frame.data()+count-primary,primary,std::min(.25f,threshold+.07f),true);
        if(secondary.valid&&secondary.frequency>estimate.frequency*1.5f){
            const auto strictEvidence=measureMonoSeries(frame.data()+count-primary,primary,estimate.frequency,analysisRate);
            if(!strictEvidence.root&&!hasPrimitiveLowSeries(strictEvidence.mask)){
                const auto higherEvidence=measureMonoSeries(frame.data()+count-primary,primary,secondary.frequency,analysisRate);
                if(higherEvidence.root&&higherEvidence.count>=3)estimate=secondary;
            }
        }
    }

    if(!estimate.valid&&freshCount>512) {
        auto bass=monoEstimator.estimate(frame.data(),count,threshold,true);
        if(bass.valid&&bass.note<36&&bass.confidence>.97f)estimate=bass;
    }
    if(held>=0&&measuredPeriod>0&&measuredPeriodNote==held){
        const float expectedPitch=float(69+12*std::log2(analysisRate/(440*measuredPeriod)));
        const auto tracked=monoEstimator.track(frame.data(),count,expectedPitch,.8f,.90f);
        if(tracked.valid){
            const bool nearby=estimate.valid&&std::abs(estimate.pitch-tracked.pitch)<.8f;
            bool octaveGlitch=false;
            if(estimate.valid&&std::abs(std::abs(estimate.pitch-tracked.pitch)-12)<.8f){
                const auto evidence=measureMonoSeries(frame.data()+count-std::min(count,512),std::min(count,512),estimate.frequency,analysisRate);
                if(estimate.pitch<tracked.pitch)octaveGlitch=tracked.confidence>.94f&&!(evidence.root&&hasPrimitiveLowSeries(evidence.mask));
                else {
                    const auto heldEvidence=measureMonoSeries(frame.data()+count-std::min(count,512),std::min(count,512),tracked.frequency,analysisRate);
                    octaveGlitch=tracked.confidence>.94f&&(heldEvidence.root||hasPrimitiveLowSeries(heldEvidence.mask));
                }
            }
            // A narrow, gain-normalised observation can retain an established
            // string through decay and timbre changes. It cannot acquire a
            // new note, and genuinely changed periods still use global YIN.
            if(!estimate.valid||octaveGlitch||(config.monoBend&&nearby)){monoTrackedOnly=!estimate.valid||octaveGlitch;estimate=tracked;}
        }
        if(config.monoBend&&estimate.valid&&lastPitchAnchor==held+lowNote&&lastSentPitch>=0){
            const int cycle=std::clamp(int(std::lround(measuredPeriod)),3,count/2);
            const auto power=[&](int start) noexcept {double sum=0,squared=0;for(int i=0;i<cycle;++i){const double x=frame[size_t(start+i)];sum+=x;squared+=x*x;}return std::max(0.,squared-sum*sum/cycle);};
            const double recent=power(count-cycle),prior=power(count-2*cycle);
            if(recent<prior*.75){
                // A window mixing a ringing note with its cutoff must not
                // manufacture a final pitch-wheel excursion. Keep the last
                // reliable oscillator during release confirmation instead.
                estimate.pitch=lastSentPitch;estimate.note=held+lowNote;
                estimate.frequency=float(440*std::pow(2.,(lastSentPitch-69.)/12.));monoTrackedOnly=true;
            }
        }
    }
    if(!estimate.valid){pitchHistoryCount=pitchHistoryWrite=0;return;}
    if(held<0&&primary>=128&&estimate.confidence>.94f){
        const auto physical=measureMonoSeries(frame.data()+count-primary,primary,estimate.frequency,analysisRate);
        monoPhysicalStable=physical.root&&physical.count>=3;
    }
    int note=estimate.note;currentPitch=estimate.pitch;view.continuousPitch=currentPitch;
    if(config.monoBend&&phaseAttack&&phaseAttackOriginPitch>=0&&std::abs(currentPitch-phaseAttackOriginPitch)>.25f){phaseAttack=false;attackRemaining=0;}
    pitchHistory[size_t(pitchHistoryWrite)]=currentPitch;pitchHistoryWrite=(pitchHistoryWrite+1)%int(pitchHistory.size());
    pitchHistoryCount=std::min(pitchHistoryCount+1,int(pitchHistory.size()));
    float minimum=currentPitch,maximum=currentPitch;
    for(int i=0;i<pitchHistoryCount;++i){minimum=std::min(minimum,pitchHistory[size_t(i)]);maximum=std::max(maximum,pitchHistory[size_t(i)]);}
    pitchHistoryRange=maximum-minimum;
    // Hold a semitone through moderate vibrato; a confirmed new pitch outside
    // this band still ends the old note before starting the new one.
    for(int n=0;n<noteCount;++n)if(voices[size_t(n)].active&&std::abs(currentPitch-(n+lowNote))<.62f)note=n+lowNote;
    if(config.monoBend&&held>=0&&std::abs(currentPitch-(held+lowNote))<=config.bendRangeSemitones+.20f
        &&!(attackRemaining>0&&!phaseAttack&&voices[size_t(held)].age>analysisRate*.07))note=held+lowNote;
    // Prediction follows continuous observed pitch; quantised MIDI identity
    // is deliberately separate from the acoustic oscillator period.
    // A single unconfirmed octave candidate must not move the tracking prior
    // itself. Update a distant new oscillator only when its Note On is
    // admitted below; close continuous pitch follows every analysis hop.
    if(held<0||note==held+lowNote){measuredPeriod=float(analysisRate)/estimate.frequency;measuredPeriodNote=note-lowNote;}
    onsetCoefficient=float(1-std::exp(-1.0/(rate*std::clamp(.5/estimate.frequency,.003,.015))));
    strength[size_t(note-lowNote)]=envelope;
    scores[size_t(note-lowNote)]=estimate.confidence;
    view.confidence=estimate.confidence;view.strongestNote=note;
}
void PitchDetector::analysePoly(int count,int,Output&) noexcept {
    if(count<512){predictionValid=false;predictionAge=0;return;}
    std::array<float,512> frame{};
    for(int i=0;i<512;++i)frame[size_t(i)]=ring[size_t((writeIndex-512+i+windowSize)%windowSize)];
    // A low spectral crest rejects broad-band unpitched input. This check is
    // independent of the harmonic model: fitting many free partials alone can
    // otherwise explain noise with a collection of spurious low pitches.
    transform.fill({});
    for(int i=0;i<512;++i)transform[size_t(i)].real=frame[size_t(i)]*window[size_t(i)];
    fft();
    float maximum=0,total=0,logarithms=0;int bins=0;
    TimeDomainPursuit::SpectralEvidence evidence;evidence.frequencies=frequency;
    // A strict stable periodic series provides positive evidence when a
    // merged fundamental would otherwise favour a neighbouring MIDI group.
    // It does not imply that the complete input is monophonic.
    auto hint=monoEstimator.estimate(frame.data(),512,.08f,true);
    if(hint.valid&&hint.confidence>.90f){
        pitchHistory[size_t(pitchHistoryWrite)]=hint.pitch;pitchHistoryWrite=(pitchHistoryWrite+1)%4;pitchHistoryCount=std::min(pitchHistoryCount+1,4);
        float lo=hint.pitch,hi=hint.pitch;for(int i=0;i<pitchHistoryCount;++i){lo=std::min(lo,pitchHistory[size_t(i)]);hi=std::max(hi,pitchHistory[size_t(i)]);}
        if(pitchHistoryCount>=3&&hi-lo<=.10f)evidence.anchorNote=hint.note-lowNote;
    }else pitchHistoryCount=pitchHistoryWrite=0;
    evidence.resolutionHz=float(analysisRate/512);evidence.maximumHz=float(analysisRate*.45);
    for(int n=0;n<noteCount;++n)evidence.active[size_t(n)]=voices[size_t(n)].active;
    const int first=std::max(1,int(50.0*fftSize/analysisRate)),last=std::min(fftSize/2,int(4500.0*fftSize/analysisRate));
    float maximumPower=0;
    for(int k=1;k<=fftSize/2;++k) {
        auto c=transform[size_t(k)];float power=c.real*c.real+c.imag*c.imag;spectrum[size_t(k)]=power;
        if(k>=2)maximumPower=std::max(maximumPower,power);
        const float magnitude=std::sqrt(power);
        if(k>=first&&k<=last) {
            maximum=std::max(maximum,power);total+=power;logarithms+=std::log(std::max(power,1e-20f));++bins;
        }
        spectralBaseline[size_t(k)]+=spectralUpdate*(magnitude-spectralBaseline[size_t(k)]);
    }
    evidence.maximumMagnitude=std::sqrt(maximumPower);
    for(int k=2;k<fftSize/2-1&&evidence.count<int(evidence.peaks.size());++k) {
        const float a=spectrum[size_t(k-1)],b=spectrum[size_t(k)],c=spectrum[size_t(k+1)];
        if(b<maximumPower*.000064f||b<=a||b<c)continue;
        const float la=std::log(std::max(a,1e-20f)),lb=std::log(std::max(b,1e-20f)),lc=std::log(std::max(c,1e-20f));
        const float denominator=la-2*lb+lc;
        const float delta=std::abs(denominator)>1e-9f?std::clamp(.5f*(la-lc)/denominator,-.5f,.5f):0;
        const float magnitude=std::sqrt(b),hz=float((k+delta)*analysisRate/fftSize);
        if(hz>evidence.maximumHz)break;
        const float novelty=std::clamp((magnitude-spectralBaseline[size_t(k)])/std::max(magnitude,1e-12f),0.0f,1.0f);
        evidence.peaks[size_t(evidence.count++)]={hz,magnitude,novelty};
    }
    // A chord's common divisor can be periodic without being a played note.
    // A newly proposed anchor needs a physical root or both coprime upper
    // partials5 and7. The latter preserves real weak/missing fundamentals.
    if(evidence.anchorNote>=0&&!voices[size_t(evidence.anchorNote)].active){
        const float target=frequency[size_t(evidence.anchorNote)];
        bool root=false,fifth=false,seventh=false;
        for(int i=0;i<evidence.count;++i){
            const auto& peak=evidence.peaks[size_t(i)];
            if(peak.magnitude>evidence.maximumMagnitude*.015f
                &&std::abs(peak.frequency-target)<target*.022f+evidence.resolutionHz*.05f)root=true;
            if(peak.magnitude<evidence.maximumMagnitude*.008f)continue;
            if(std::abs(peak.frequency-target*5)<target*5*.015f)fifth=true;
            if(std::abs(peak.frequency-target*7)<target*7*.015f)seventh=true;
        }
        if(!root&&!(fifth&&seventh))evidence.anchorNote=-1;
    }
    evidence.sortedPeaks=true;
    polyAnchor=evidence.anchorNote;
    if(polyAnchor>=0){measuredPeriod=float(analysisRate)/hint.frequency;measuredPeriodNote=polyAnchor;}
    const float flatness=std::exp(logarithms/std::max(bins,1))/(total/std::max(bins,1)+1e-20f);
    const float crest=maximum/(total/std::max(bins,1)+1e-20f);
    if((flatness>.12f&&polyAnchor<0)||crest<6.0f){predictionValid=false;predictionAge=0;predictionFast=predictionSlow=predictionObserved=predictionExpected=0;return;}
    // New acquisitions use model-prediction innovation, not gross amplitude
    // beating. Independent new spectral lines can admit a quiet added voice.
    bool anyActive=false;for(const auto& voice:voices)anyActive|=voice.active;
    if(!anyActive)polyAdmission=std::max(polyAdmission,int(analysisRate*.120));
    pursuit.analyse(frame.data(),512,strength,scores,config.polyphony,config.sensitivity,&evidence);
    predictionValid=pursuit.copyPrediction(polyPrediction);predictionIndex=0;
    if(!predictionValid){predictionAge=0;predictionFast=predictionSlow=predictionObserved=predictionExpected=0;}
    for(int n=0;n<noteCount;++n)if(strength[size_t(n)]>0&&!voices[size_t(n)].active)
        independentNovelty[size_t(n)]=TimeDomainPursuit::hasIndependentPeaks(n,evidence,evidence.active,true);
    view.confidence=0;view.strongestNote=-1;
    float strongest=0;
    for(int n=0;n<noteCount;++n) {
        const float minimum=voices[size_t(n)].active?gate*.08f:gate*.8f;
        if(strength[size_t(n)]<minimum){strength[size_t(n)]=0;scores[size_t(n)]=0;}
        if(strength[size_t(n)]>strongest){strongest=strength[size_t(n)];view.strongestNote=n+lowNote;}
        view.confidence=std::max(view.confidence,scores[size_t(n)]);
    }
    // A one-voice phase reset may be a same-pitch repick or a new note.
    // Retain the old voice until a complete fresh observation plus confirming
    // hops identifies the same source. Added voices and pitch changes cancel.
    if(pendingPolyRepick>=0){
        int held=0;for(const auto& voice:voices)held+=voice.active;
        const int target=pendingPolyRepick;
        if(held!=1||!voices[size_t(target)].active)pendingPolyRepick=-1;
        else if(pendingPolyRepickAge>=std::max(512+2*hopSize,int(std::ceil(3*pendingPhasePeriod)))&&polyAnchor>=0){
            if(polyAnchor!=target)pendingPolyRepick=-1;
            else if(strength[size_t(target)]>gate*.8f&&confirmations[size_t(target)]>=5&&fastEnvelope>slowEnvelope*.8f){
                const int n=int(std::lround(pendingPhasePeriod));
                const int past=int(std::lround(3*pendingPhasePeriod)),recent=int(std::lround(2*pendingPhasePeriod));
                const double extrapolate=double(pendingPolyRepickAge+recent-n+1)/std::max(1,past-recent);
                const double floor=double(gate)*gate*n*n*.25;
                double fundamentalDistance=0;int changedUpperOdd=0;
                for(int group=0;group<4;++group){const int harmonic=2*group+1;if(harmonic>=pendingPhasePeriod*.45)continue;
                    const auto after=projectPhase(ring,writeIndex-n,n,double(pendingPolyRepickAge-n+1),pendingPhasePeriod,harmonic);
                    const int offset=4*group;const double ar=pendingPhaseCoefficients[size_t(offset)],ai=pendingPhaseCoefficients[size_t(offset+1)],br=pendingPhaseCoefficients[size_t(offset+2)],bi=pendingPhaseCoefficients[size_t(offset+3)];
                    const double a=ar*ar+ai*ai,b=br*br+bi*bi,c=after[0]*after[0]+after[1]*after[1];
                    if(a<=floor||b<=floor||c<=floor||a>4*b||b>4*a)continue;
                    const double trend=std::atan2(ar*bi-ai*br,ar*br+ai*bi),angle=trend*extrapolate;
                    const double er=br*std::cos(angle)-bi*std::sin(angle),ei=br*std::sin(angle)+bi*std::cos(angle);
                    const double distance=2-2*std::clamp((er*after[0]+ei*after[1])/std::sqrt(b*c),-1.,1.);
                    if(group==0)fundamentalDistance=distance;else if(distance>.08)++changedUpperOdd;
                }
                const bool phaseChanged=fundamentalDistance>.08||changedUpperOdd>=2;
                pendingPolyRepick=-1;
                if(phaseChanged){phaseAttack=true;attackRemaining=hopSize;polyAdmission=int(analysisRate*.120);
                    for(int j=0;j<noteCount;++j)retriggerBaseline[size_t(j)]=voices[size_t(j)].strength;}

            }
        }
    }
}
void PitchDetector::updateVoices(int offset,Output& out) noexcept {
    int chosen=-1;
    const bool bendEnabled=config.monoBend&&!config.polyphonic;
    const auto monoStable=[&](int n) noexcept {
        return signalSamples>=analysisRate/frequency[size_t(n)]*2.25&&pitchHistoryCount>=3
            &&pitchHistoryRange<=(bendEnabled?.8f:monoPhysicalStable?.25f:.10f);
    };
    for(int n=0;n<noteCount;++n) {
        if(strength[size_t(n)]>0) {
            if(!config.polyphonic&&previousPitch>=0&&currentPitch>=0&&std::abs(currentPitch-previousPitch)>(bendEnabled?.8f:.22f))confirmations[size_t(n)]=1;
            else confirmations[size_t(n)]=std::min(confirmations[size_t(n)]+1,64);
        }else confirmations[size_t(n)]=0;
        if(confirmations[size_t(n)]>=(config.polyphonic?(polyAnchor>=0?6:10):3)
            &&(config.polyphonic||monoStable(n))&&(chosen<0||strength[size_t(n)]>strength[size_t(chosen)]))chosen=n;
    }
    // In mono, a confirmed change is legato: old note off precedes new note on.
    if(!config.polyphonic&&chosen>=0)for(int n=0;n<noteCount;++n)if(n!=chosen&&voices[size_t(n)].active){emit(false,n+lowNote,0,offset,out);voices[size_t(n)]={};}
    for(int n=0;n<noteCount;++n) {
        auto& voice=voices[size_t(n)];float detected=strength[size_t(n)];
        const bool ready=confirmations[size_t(n)]>=(config.polyphonic?(polyAnchor>=0?6:10):3)
            &&(config.polyphonic||monoStable(n));
        if(detected>0) {
            voice.absent=0;
            if(voice.active&&ready&&(config.polyphonic||!monoTrackedOnly)&&(!config.polyphonic||polyAdmission>0||independentNovelty[size_t(n)])&&attackRemaining>0&&voice.age>analysisRate*.07&&((phaseAttack&&fastEnvelope>slowEnvelope*.8f)||(!phaseAttack&&detected>retriggerBaseline[size_t(n)]*1.35f))) {
                emit(false,n+lowNote,0,offset,out);voice.active=false;
                retriggerBaseline[size_t(n)]=std::numeric_limits<float>::infinity();
            }
            if(!voice.active&&ready&&detected>gate*(config.polyphonic?.8f:1.0f)
                &&(!config.polyphonic||polyAdmission>0||independentNovelty[size_t(n)])&&fastEnvelope>slowEnvelope*.35f) {
                if(config.polyphonic) {
                    int held=0,weakest=-1;
                    for(int j=0;j<noteCount;++j)if(voices[size_t(j)].active){++held;
                        if(strength[size_t(j)]==0&&voices[size_t(j)].absent>=std::min(releaseSamples/2,int(analysisRate*.015))&&(weakest<0||voices[size_t(j)].strength<voices[size_t(weakest)].strength))weakest=j;}
                    if(held>=config.polyphony) {
                        if(weakest<0)continue;
                        emit(false,weakest+lowNote,0,offset,out);voices[size_t(weakest)]={};
                    }
                }
                // Root-sum amplitude tracks independent polyphonic velocities.
                int velocity=std::clamp(int(std::lround(127.0f*std::pow(std::min(detected*2.0f,1.0f),.45f))),1,127);
                if(bendEnabled)emitPitch(n+lowNote,currentPitch,offset,out);
                emit(true,n+lowNote,velocity,offset,out);voice.active=true;voice.age=0;
                if(!config.polyphonic){phaseAttack=false;attackRemaining=0;measuredPeriod=float(analysisRate/(440*std::pow(2.,(currentPitch-69.)/12.)));measuredPeriodNote=n;}
            }
            voice.strength=detected;
            if(voice.active&&bendEnabled)emitPitch(n+lowNote,currentPitch,offset,out);
        } else if(voice.active) {
            voice.absent+=hopSize;
            if(voice.absent>=releaseSamples){emit(false,n+lowNote,0,offset,out);voice={};if(bendEnabled){lastPitchAnchor=-1;lastSentPitch=-1;}}
        }
        if(voice.active)voice.age=std::min(voice.age+hopSize,windowSize*16);
    }
    previousPitch=currentPitch;
    view.activeNotes=0;view.activePitchClasses=0;
    for(int n=0;n<noteCount;++n)if(voices[size_t(n)].active){++view.activeNotes;view.activePitchClasses=uint16_t(view.activePitchClasses|(1u<<((n+lowNote)%12)));}
    if(view.activeNotes==0&&envelope<=gate){view.confidence=0;view.strongestNote=-1;view.continuousPitch=-1;}
}
}
