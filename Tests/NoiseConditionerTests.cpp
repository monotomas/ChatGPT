// SPDX-License-Identifier: AGPL-3.0-or-later
// Stationary interference, relative-level evidence and preservation controls.
// These controlled signals test mechanism; recorded handling-noise acceptance
// belongs to the independent musical-readiness corpus.
#include "Pitch/NoiseConditioner.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr double pi=3.1415926535897932384626433832795;
unsigned checks=0;
void check(bool ok,const std::string& text){++checks;if(!ok)throw std::runtime_error(text);}
struct Random {uint32_t value=0x152b37af;double next(){value=1664525*value+1013904223;return (double(value)/4294967296.-.5)*2;}};
double hum(double time,double frequency){return .03*(std::sin(2*pi*frequency*time)+.4*std::sin(2*pi*2*frequency*time+.3)+.2*std::sin(2*pi*3*frequency*time+.7));}
double guitar(double time,double frequency){return .003*(std::sin(2*pi*frequency*time)+.35*std::sin(4*pi*frequency*time+.2)+.12*std::sin(6*pi*frequency*time+.4));}
template<size_t N>void rotate(std::array<float,N>& frame,int end){std::rotate(frame.begin(),frame.begin()+end%int(N),frame.end());}

void stationaryMains(){
    for(double rate:{8000.,12000.,24000.})for(double frequency:{49.95,50.,50.05,59.95,60.,60.05}){
        flysnapper::pitch::NoiseConditioner n(rate);double before=0,after=0;
        const int frames=int(rate*3);
        for(int i=0;i<frames;++i){const double x=hum(i/rate,frequency);const double y=n.processSample(float(x));
            if(i>rate*2){before+=x*x;after+=y*y;}}
        const double reduction=10*std::log10(after/before);
        check(reduction<-30,"Stationary mains cancellation did not reach30dB");
        std::cout<<"hum,"<<rate<<','<<frequency<<",attenuation_db,"<<reduction<<'\n';
    }
    // Simultaneous line families share300Hz. A joint residual must not
    // double-subtract that line or destabilise the learned model.
    flysnapper::pitch::NoiseConditioner n(12000);double a=0,b=0;
    for(int i=0;i<48000;++i){const double t=i/12000.,x=hum(t,50.03)+.7*hum(t,59.97);const auto y=n.processSample(float(x));if(i>=36000){a+=x*x;b+=y*y;}}
    std::cout<<"mixed,"<<10*std::log10(b/a)<<",freq,"<<n.mainsFrequency(0)<<","<<n.mainsFrequency(1)<<"\n";
    check(10*std::log10(b/a)<-25,"Mixed50/60Hz families failed joint cancellation");
}
void noNotchOnProtectedMusic(){
    for(double hz:{49.,50.,58.27047,60.,61.735413,82.406889,110.,196.,440.}){
        flysnapper::pitch::NoiseConditioner n(12000);double before=0,after=0;
        for(int i=0;i<24000;++i){const double x=guitar(i/12000.,hz),y=n.processSample(float(x),true);
            before+=x*x;after+=y*y;}
        check(std::abs(10*std::log10(after/before))<.001,"Protected musical tone was learned away/notched");
    }
}
void quietGuitarWithLoudHum(){
    for(double hz:{49.,50.,61.735413,82.406889,100.,110.,196.}){
        flysnapper::pitch::NoiseConditioner n(12000);Random random;
        for(int i=0;i<36000;++i){const double t=i/12000.;n.processSample(float(hum(t,50.05)+random.next()*.00005));}
        double noisy=0,conditioned=0,wanted=0;
        for(int i=0;i<12000;++i){const double t=3+i/12000.,tone=guitar(i/12000.,hz)*std::exp(-i/12000.);
            const double noise=hum(t,50.05)+random.next()*.00005,x=tone+noise;
            const double y=n.processSample(float(x),true,i<20);
            noisy+=noise*noise;conditioned+=(y-tone)*(y-tone);wanted+=tone*tone;
        }
        const double improvement=10*std::log10(noisy/conditioned);
        check(improvement>30,"Frozen learned mains did not reveal quiet musical tone");
        check(conditioned<wanted*.03,"Noise-conditioned quiet guitar remained masked");
        std::cout<<"quiet_guitar,"<<hz<<",noise_improvement_db,"<<improvement<<'\n';
    }
}
void gainRelativeEvidence(){
    std::array<flysnapper::pitch::NoiseConditioner::Evidence,3> observations{};
    int index=0;
    for(double gain:{.1,1.,10.}){
        flysnapper::pitch::NoiseConditioner n(12000);Random random;std::array<float,512> frame{};
        for(int i=0;i<36000;++i){const double t=i/12000.;n.processSample(float(gain*(hum(t,50)+random.next()*.00010)));}
        for(int i=0;i<12000;++i){const double t=3+i/12000.;const double tone=guitar(i/12000.,110.);
            frame[size_t(i%512)]=n.processSample(float(gain*(tone+hum(t,50)+random.next()*.00010)),true);}
        rotate(frame,12000);observations[size_t(index++)]=n.assess(frame.data(),512,110,.99f,true);
    }
    for(int i=0;i<3;++i){const auto& e=observations[size_t(i)];
        check(e.harmonicFraction>.97f&&e.phaseCoherence>.96f,"Quiet periodic guitar did not produce coherent musical evidence");
        std::cout<<"gain_evidence,"<<i<<",snr,"<<e.snrDb<<",harmonic,"<<e.harmonicFraction<<",phase,"<<e.phaseCoherence<<",floor,"<<e.backgroundRms<<",residual,"<<e.residualRms<<"\n";
        check(e.snrDb>22,"Quiet periodic guitar lost noise-relativeSNR");
        check(!e.clearlyUnpitched&&!e.mainsOnly,"Quiet legitimate tone was classified as noise");
        check(std::abs(e.snrDb-observations[1].snrDb)<.02f,"Noise evidence changed under common gain");
        check(std::abs(e.backgroundSnrDb-observations[1].backgroundSnrDb)<.02f,"Idle-floor evidence changed under common gain");
    }
}
void stringModelResidualIsNotBackgroundNoise(){
    flysnapper::pitch::NoiseConditioner n(12000);Random random;std::array<float,512> frame{};
    for(int i=0;i<24000;++i)n.processSample(float(random.next()*.00005));
    for(int i=0;i<512;++i){const double t=i/12000.;
        const double body=.0042*std::sin(2*pi*137.47*t+.17);
        frame[size_t(i)]=n.processSample(float(guitar(t,82.406889)+body+random.next()*.00005),true);
    }
    const auto e=n.assess(frame.data(),512,82.406889f,.95f,true);
    check(e.harmonicSnrDb<6,"Control failed to exercise significant nonharmonic string/body energy");
    check(e.backgroundSnrDb>30,"Legitimate model residual was incorrectly included in learned idle noise");
    check(e.snrDb<=e.harmonicSnrDb+.001f,"Conservative compatibility SNR exceeded the pure harmonic model ratio");
    n.reset();for(int i=0;i<24000;++i)n.processSample(float(random.next()*.01));
    for(int i=0;i<512;++i)frame[size_t(i)]=float(guitar(i/12000.,110)*.1);
    const auto decay=n.assess(frame.data(),512,110,.99f,true);
    check(decay.snrDb<0&&decay.backgroundSnrDb<0,"Control failed to exercise a stale high idle floor");
    check(decay.harmonicSnrDb>50,"Stale idle floor censored the independent harmonic fit ratio");
}
void coldMainsAndUnpitchedEvidence(){
    flysnapper::pitch::NoiseConditioner n(12000);
    for(double hz:{50.,60.,100.,120.}){
        std::array<float,512> frame{};
        const double family=(hz==50||hz==100)?50.:60.;
        for(int i=0;i<512;++i)frame[size_t(i)]=float(hum(i/12000.,family));
        const auto e=n.assess(frame.data(),512,float(hz),.99f,false,false);
        check(e.mainsCandidate&&e.mainsOnly,"Cold uncalibrated mains was not identified");
        check(!n.assess(frame.data(),512,float(hz),.99f,true,false).mainsOnly,"Existing bent/held note was unconditionally mains-rejected");
        check(!n.assess(frame.data(),512,float(hz),.99f,false,true).mainsOnly,"Independent musical excitation could not override mains suspicion");
    }
    Random random;std::array<float,512> noise{};
    for(auto& x:noise)x=float(random.next()*.03);
    const auto e=n.assess(noise.data(),512,196,.3f,false,false);
    check(e.clearlyUnpitched&&e.harmonicFraction<.2f,"Broadband handling-like noise received tonal salience");
}
void switchingMainsIsNotMusicalExcitation(){
    for(double hz:{50.,60.,100.,120.})for(double ramp:{0.,.005,.050,.200})for(double gain:{.1,1.,10.}){
        flysnapper::pitch::NoiseConditioner n(12000);std::array<float,1024> frame{};
        const int count=int(12000*2.4/hz);
        for(int i=0;i<count;++i){const double t=i/12000.,envelope=ramp>0?std::min(1.,t/ramp):1.;
            frame[size_t(i)]=float(gain*.03*envelope*(std::sin(2*pi*hz*t)+.4*std::sin(4*pi*hz*t+.3)+.2*std::sin(6*pi*hz*t+.7)));}
        const auto e=n.assess(frame.data(),count,float(hz),.99f,false,false);
        check(e.mainsOnly,"A switched/ramped mains level was mistaken for independent musical excitation");
    }
}
void residualHighMainsCannotBecomeANote(){
    // Cover the entire learning interval, not just the raw startup frame.
    // When low harmonics cancel first, high-order residual buzz remains
    // periodic and must not be reinterpreted as a newly played string.
    for(double base:{50.,60.})for(double gain:{.1,1.,10.}){
        flysnapper::pitch::NoiseConditioner n(12000);std::array<float,512> ring{},frame{};
        int write=0,failures=0;
        for(int i=0;i<25200;++i){const double time=i/12000.,phase=2*pi*(base*time+.5*.13*time*time/2.1);
            double x=0;for(int h=1;h<=24;++h)x+=std::sin(h*phase+h*.718)/std::pow(double(h),1.38);
            ring[size_t(write)]=n.processSample(float(x*.003*gain));write=(write+1)%512;
            if(i<511||i%32)continue;
            for(int k=0;k<512;++k)frame[size_t(k)]=ring[size_t((write+k)%512)];
            const auto e=n.assess(frame.data(),512,float(base+.13*time/2.1),.99f);
            if(e.snrDb>=6&&e.harmonicFraction>.4&&e.phaseCoherence>.95&&!e.mainsOnly)++failures;
        }
        check(failures==0,"Uncancelled high mains harmonics escaped full-stream admission evidence");
    }
    // Neighboring fretted bass notes must retain their identity even when
    // their short-window waveform also correlates with a nearby mains cycle.
    flysnapper::pitch::NoiseConditioner n(12000);std::array<float,512> frame{};
    for(double frequency:{48.999429,58.27047,61.735413,97.998859,116.54094,123.470825}){
        for(int i=0;i<512;++i)frame[size_t(i)]=float(guitar(i/12000.,frequency));
        check(!n.assess(frame.data(),512,float(frequency),.99f).mainsOnly,"Mains evidence rejected an adjacent fretted note");
    }
}
void stoppedSourceCannotSynthesizeHeldHum(){
    for(double base:{49.95,50.,50.05,59.95,60.,60.05})for(bool filterTail:{false,true}){
        flysnapper::pitch::NoiseConditioner n(12000);
        for(int i=0;i<36000;++i)n.processSample(float(hum(i/12000.,base)));
        double tail=0,peak=0;
        for(int i=0;i<3000;++i){
            // Ableton/our DC-removal filter can leave a nonzero exponential
            // transient even after its source buffer has become exactly zero.
            const double raw=filterTail?.004*std::exp(-2*pi*12*i/12000.):0.;
            const double out=n.processSample(float(raw),true);
            if(i>=1800){tail+=out*out;peak=std::max(peak,std::abs(out));}
        }
        check(peak<.000001&&tail<1e-9,"A stale protected hum model generated audio after the real input stopped");
    }
}
void knownIdleNoiseIsNotASecondVoice(){
    for(double base:{50.,60.})for(double gain:{.1,1.,10.})for(double snr:{6.,9.,12.}){
        flysnapper::pitch::NoiseConditioner n(12000);Random random;std::array<float,512> frame{};
        const double humScale=.0001/(.03*std::sqrt(.6)),noiseRms=.0001/std::pow(10.,snr/20);
        for(int i=0;i<36000;++i)n.processSample(float(gain*(hum(i/12000.,base)*humScale+random.next()*noiseRms*std::sqrt(3.))));
        for(int i=0;i<512;++i)frame[size_t(i)]=float(gain*(hum((36000+i)/12000.,base)*humScale+random.next()*noiseRms*std::sqrt(3.)));
        const auto e=n.assess(frame.data(),512,float(base),.99f);
        check(e.mainsOnly,"Known mains plus its already learned white floor became a fresh tonal source");
        for(int i=0;i<512;++i)frame[size_t(i)]=float(gain*random.next()*noiseRms*std::sqrt(3.));
        check(!n.assess(frame.data(),512,float(base),.99f).mainsOnly,"Idle-floor allowance mislabeled broad noise as a stationary mains comb");
        for(int i=0;i<512;++i)frame[size_t(i)]=float(gain*(guitar(i/12000.,48.999429)*.1+random.next()*noiseRms*std::sqrt(3.)));
        check(!n.assess(frame.data(),512,48.999429f,.99f).mainsOnly,"Known floor allowance rejected a quiet neighboring guitar note");
    }
}
void ordinaryHighGuitarNotesAreNotMainsCarriers(){
    for(double hz:{349.228231,659.255114,698.456463})for(bool learnedHum:{false,true}){
        flysnapper::pitch::NoiseConditioner n(12000);std::array<float,512> frame{};
        const double base=hz==659.255114?60.:50.;
        const auto fullHum=[&](double t){double x=0;for(int h=1;h<=16;++h)x+=.03*std::sin(2*pi*base*h*t+h*.718)/std::pow(double(h),1.38);return x;};
        if(learnedHum)for(int i=0;i<36000;++i)n.processSample(float(fullHum(i/12000.)));
        for(int i=0;i<512;++i)frame[size_t(i)]=learnedHum?n.processSample(float(fullHum((36000+i)/12000.)+guitar(i/12000.,hz)),true):float(guitar(i/12000.,hz));
        const auto e=n.assess(frame.data(),512,float(hz),.99f);
        check(!e.mainsOnly,"An ordinary high guitar note was mislabeled as a high-order mains carrier");
    }
}
void finiteAndBounded(){
    flysnapper::pitch::NoiseConditioner n(12000);
    for(float x:{0.f,std::nanf(""),INFINITY,-INFINITY,1e30f,-1e30f})check(std::isfinite(n.processSample(x)),"Nonfinite conditioner output");
    const auto e=n.assess(nullptr,0,std::nanf(""),std::nanf(""));check(std::isfinite(e.snrDb),"Invalid analysis produced nonfinite evidence");
}
}
int main(){try{stationaryMains();noNotchOnProtectedMusic();quietGuitarWithLoudHum();gainRelativeEvidence();stringModelResidualIsNotBackgroundNoise();coldMainsAndUnpitchedEvidence();switchingMainsIsNotMusicalExcitation();residualHighMainsCannotBecomeANote();stoppedSourceCannotSynthesizeHeldHum();knownIdleNoiseIsNotASecondVoice();ordinaryHighGuitarNotesAreNotMainsCarriers();finiteAndBounded();
    std::cout<<"NoiseConditioner: PASS ("<<checks<<" mechanism checks; not recorded guitar/noise acceptance).\n";return 0;
}catch(const std::exception& e){std::cerr<<"NoiseConditioner: FAIL: "<<e.what()<<'\n';return 1;}}
