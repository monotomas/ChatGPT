// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PolyphonicTracker.h"
#include "TimeDomainPursuit.h"
#include "YinEstimator.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace flysnapper::pitch {
namespace {
constexpr int fftSize=2048,harmonics=8,columns=16,maxJoint=96;
constexpr double pi=3.1415926535897932384626433832795;
double hz(double pitch) noexcept{return 440*std::pow(2.,(pitch-69)/12);}
double pitch(double frequency) noexcept{return 69+12*std::log2(frequency/440);}
struct Complex {float re=0,im=0;};
struct Pair {double sine=0,cosine=0;};
Pair sums(double phase,int count) noexcept {
    phase=std::remainder(phase,2*pi);
    if(std::abs(phase)<1e-10)return {phase*count*(count-1)*.5,double(count)};
    const double factor=std::sin(count*phase*.5)/std::sin(phase*.5),center=(count-1)*phase*.5;
    return {factor*std::sin(center),factor*std::cos(center)};
}
struct Model {
    double frequency=0,stiffness=0;
    int count=0;
    std::array<float,columns*PolyphonicTracker::maximumSamples> basis{};
    std::array<double,columns> omega{},mean{},weight{},coefficient{},projection{};
    std::array<bool,columns> valid{};
    void prepare(double f,double rate,int samples,double stiff=0) noexcept {
        frequency=f;stiffness=stiff;count=samples;
        for(int c=0;c<columns;++c){
            const int h=c%harmonics+1;const bool cosine=c>=harmonics;
            const double partial=f*h*std::sqrt((1+stiff*h*h)/(1+stiff));
            valid[size_t(c)]=partial<rate*.45;weight[size_t(c)]=1/std::sqrt(double(h));
            omega[size_t(c)]=2*pi*partial/rate;
            if(!valid[size_t(c)]){mean[size_t(c)]=0;continue;}
            const auto sum=sums(omega[size_t(c)],samples);mean[size_t(c)]=(cosine?sum.cosine:sum.sine)/samples;
            const double cs=std::cos(omega[size_t(c)]),sn=std::sin(omega[size_t(c)]);double re=1,im=0;
            for(int n=0;n<samples;++n){
                basis[size_t(c*PolyphonicTracker::maximumSamples+n)]=float(((cosine?re:im)-mean[size_t(c)])*weight[size_t(c)]);
                const double next=re*cs-im*sn;im=re*sn+im*cs;re=next;
            }
        }
    }
};
double cross(const Model& a,int ca,const Model& b,int cb) noexcept {
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
bool solve(std::array<double,maxJoint*maxJoint>& matrix,const std::array<double,maxJoint>& rhs,
           std::array<double,maxJoint>& result,int count) noexcept {
    std::array<double,maxJoint> intermediate{};
    for(int r=0;r<count;++r)for(int c=0;c<=r;++c){
        double value=matrix[size_t(r*maxJoint+c)];
        for(int k=0;k<c;++k)value-=matrix[size_t(r*maxJoint+k)]*matrix[size_t(c*maxJoint+k)];
        if(r==c){if(value<=1e-12||!std::isfinite(value))return false;matrix[size_t(r*maxJoint+c)]=std::sqrt(value);}
        else matrix[size_t(r*maxJoint+c)]=value/matrix[size_t(c*maxJoint+c)];
    }
    for(int r=0;r<count;++r){double value=rhs[size_t(r)];for(int c=0;c<r;++c)value-=matrix[size_t(r*maxJoint+c)]*intermediate[size_t(c)];intermediate[size_t(r)]=value/matrix[size_t(r*maxJoint+r)];}
    for(int r=count-1;r>=0;--r){double value=intermediate[size_t(r)];for(int c=r+1;c<count;++c)value-=matrix[size_t(c*maxJoint+r)]*result[size_t(c)];result[size_t(r)]=value/matrix[size_t(r*maxJoint+r)];}
    return true;
}
double amplitude(const Model& m,int harmonic) noexcept {
    const auto a=m.coefficient[size_t(harmonic)],b=m.coefficient[size_t(harmonic+harmonics)];
    return std::sqrt(a*a+b*b)*m.weight[size_t(harmonic)];
}
}

struct PolyphonicTracker::Data {
    struct Peak {double frequency=0,power=0,phaseFrequency=0;};
    struct Candidate {double frequency=0,stiffness=0,score=0;int support=0;bool root=false,legacy=false;};
    struct State {Voice view;int absent=0,pending=0;double previousPitch=-1,velocity=0;std::array<double,harmonics> profile{};std::array<double,2*harmonics> phaseEnd{};int phaseCount=0;double phaseFrequency=0;};
    double rate=12000;
    std::array<Complex,fftSize> transform{},previous{};
    std::array<Complex,fftSize/2> twiddle{};
    std::array<int,fftSize> reverse{};
    std::array<std::array<float,maximumSamples>,16> windows{};
    std::array<float,maximumSamples> wave{},residual{},isolated{};
    std::array<double,fftSize/2+1> powers{};
    std::array<Peak,128> peaks{};int peakCount=0,lastCount=0;
    bool havePrevious=false;
    std::array<Candidate,96> candidates{};int candidateCount=0;
    std::array<Model,maximumVoices> models{};
    struct Observation {std::array<double,2*harmonics> phase{};double frequency=0,confidence=0,innovation=0;int count=0;};
    std::array<Observation,maximumVoices> observations{};
    Model scratch;
    std::array<State,maximumVoices> states{};
    std::array<double,maxJoint*maxJoint> matrix{};
    std::array<double,maxJoint> rhs{},solution{};
    TimeDomainPursuit legacy,legacyShort;
    YinEstimator periodicity;
    std::array<float,73> legacyStrength{},legacyConfidence{};std::array<int,73> confirmations{};std::array<float,4> hintHistory{};int hintCount=0,hintWrite=0;
    int lastHop=32,freshSamples=0,quietSamples=0,admissionSuppressed=0;double energy=0;
    double lastRecentPower=0;

    void fft() noexcept {
        for(int i=0;i<fftSize;++i)if(i<reverse[size_t(i)])std::swap(transform[size_t(i)],transform[size_t(reverse[size_t(i)])]);
        for(int length=2;length<=fftSize;length*=2){const int half=length/2,stride=fftSize/length;
            for(int start=0;start<fftSize;start+=length)for(int k=0;k<half;++k){
                const auto w=twiddle[size_t(k*stride)],b=transform[size_t(start+k+half)],a=transform[size_t(start+k)];
                const Complex p{b.re*w.re-b.im*w.im,b.re*w.im+b.im*w.re};
                transform[size_t(start+k)]={a.re+p.re,a.im+p.im};transform[size_t(start+k+half)]={a.re-p.re,a.im-p.im};
            }
        }
    }
    void spectrum(int count,int hop) noexcept {
        transform.fill({});const auto& window=windows[size_t(count/32-1)];
        for(int i=0;i<count;++i)transform[size_t(i)].re=wave[size_t(i)]*window[size_t(i)];
        fft();double maximum=0;
        for(int k=0;k<=fftSize/2;++k){const auto c=transform[size_t(k)];powers[size_t(k)]=double(c.re)*c.re+double(c.im)*c.im;maximum=std::max(maximum,powers[size_t(k)]);}
        peakCount=0;
        for(int k=3;k<fftSize/2-1&&peakCount<int(peaks.size());++k){
            const double a=powers[size_t(k-1)],b=powers[size_t(k)],c=powers[size_t(k+1)];
            if(b<=a||b<c||b<maximum*.000025)continue;
            const double la=std::log(std::max(a,1e-30)),lb=std::log(std::max(b,1e-30)),lc=std::log(std::max(c,1e-30));
            const double curvature=la-2*lb+lc,offset=curvature< -1e-12?std::clamp(.5*(la-lc)/curvature,-.5,.5):0;
            double frequency=(k+offset)*rate/fftSize,phaseFrequency=frequency;
            if(havePrevious&&lastCount==count&&hop>0){
                const auto z=transform[size_t(k)],p=previous[size_t(k)];
                if(double(p.re)*p.re+double(p.im)*p.im>b*.04){
                    const double phase=std::atan2(double(z.im)*p.re-double(z.re)*p.im,double(z.re)*p.re+double(z.im)*p.im);
                    const double delta=std::remainder(phase-2*pi*k*hop/fftSize,2*pi);
                    const double precise=k*rate/fftSize+delta*rate/(2*pi*hop);
                    if(std::abs(precise-frequency)<rate/count*.45)phaseFrequency=precise;
                }
            }
            if(frequency<30||frequency>rate*.45)continue;
            peaks[size_t(peakCount++)]={frequency,b,phaseFrequency};
        }
        previous=transform;havePrevious=true;lastCount=count;lastHop=hop;
    }
    int nearest(double frequency,double tolerance) const noexcept {
        int best=-1;double error=tolerance;
        for(int i=0;i<peakCount;++i){const double e=std::abs(peaks[size_t(i)].frequency-frequency);if(e<error){best=i;error=e;}}
        return best;
    }
    Candidate assess(double frequency,int count) const noexcept {
        Candidate result;result.frequency=frequency;
        if(frequency<hz(27.5)||frequency>hz(100.5))return result;
        double sum=0,weight=0;std::array<int,12> matches{};matches.fill(-1);
        for(int pass=0;pass<2;++pass){
            sum=weight=0;result.score=0;result.support=0;result.root=false;
            for(int h=1;h<=12&&frequency*h<rate*.45;++h){
                const double target=frequency*h,tolerance=std::min(frequency*.30,std::max(target*.022,rate/count*.16));
                const int found=nearest(target,tolerance);matches[size_t(h-1)]=found;
                if(found<0)continue;
                const auto& peak=peaks[size_t(found)];const double w=peak.power/std::sqrt(double(h));
                sum+=peak.phaseFrequency/h*w;weight+=w;result.score+=w;++result.support;if(h==1)result.root=true;
            }
            if(weight>0)frequency=sum/weight;
        }
        result.frequency=frequency;
        // A chord's common divisor can have many upper matches. A new source
        // requires its own physical root; later joint modelling can preserve a
        // held source when that root becomes weak, rather than inventing it.
        if(!result.root||result.support<2){result.score=0;return result;}
        const auto root=matches[0];double maxPower=0;
        for(int i=0;i<peakCount;++i)maxPower=std::max(maxPower,peaks[size_t(i)].power);
        if(root<0||peaks[size_t(root)].power<maxPower*.0004)result.score=0;
        return result;
    }
    void addCandidate(double frequency,int count,bool fromLegacy=false) noexcept {
        auto candidate=assess(frequency,count);candidate.legacy=fromLegacy;
        if(candidate.score<=0&&!fromLegacy)return;
        if(fromLegacy&&candidate.score<=0){candidate.frequency=frequency;candidate.score=1e-12;}
        for(int i=0;i<candidateCount;++i)if(std::abs(pitch(candidates[size_t(i)].frequency)-pitch(candidate.frequency))<.20){
            if(candidate.score>candidates[size_t(i)].score)candidates[size_t(i)]=candidate;
            else candidates[size_t(i)].legacy|=fromLegacy;
            return;
        }
        if(candidateCount<int(candidates.size()))candidates[size_t(candidateCount++)]=candidate;
    }
    bool fit(Model* models,int used,const float* input,int count) noexcept {
        const int dimensions=used*columns;rhs.fill(0);solution.fill(0);
        for(int j=0;j<used;++j){auto& m=models[j];
            for(int c=0;c<columns;++c){double projection=0;if(m.valid[size_t(c)])for(int n=0;n<count;++n)projection+=double(m.basis[size_t(c*maximumSamples+n)])*input[n];rhs[size_t(j*columns+c)]=projection;}
        }
        for(int r=0;r<dimensions;++r)for(int c=0;c<=r;++c)
            matrix[size_t(r*maxJoint+c)]=cross(models[r/columns],r%columns,models[c/columns],c%columns)+(r==c?count*.002:0);
        if(!solve(matrix,rhs,solution,dimensions))return false;
        for(int j=0;j<used;++j)for(int c=0;c<columns;++c)models[j].coefficient[size_t(c)]=solution[size_t(j*columns+c)];
        return true;
    }
    double modelValue(const Model& m,int sample) const noexcept {
        double value=0;for(int c=0;c<columns;++c)if(m.valid[size_t(c)])value+=m.basis[size_t(c*maximumSamples+sample)]*m.coefficient[size_t(c)];return value;
    }
    double residualFor(int used,int count) noexcept {
        double energy=0;
        for(int n=0;n<count;++n){double value=wave[size_t(n)];for(int j=0;j<used;++j)value-=modelValue(models[size_t(j)],n);residual[size_t(n)]=float(value);energy+=value*value;}
        return energy;
    }
    double refine(Model& m,const float* input,int count) noexcept {
        scratch.prepare(m.frequency,rate,count,m.stiffness);
        if(!fit(&scratch,1,input,count))return m.frequency;
        double numerator=0,denominator=0;
        for(int n=0;n<count;++n){
            double derivative=0;
            for(int h=0;h<harmonics;++h)if(scratch.valid[size_t(h)]){
                const double sine=scratch.basis[size_t(h*maximumSamples+n)]+scratch.mean[size_t(h)]*scratch.weight[size_t(h)];
                const double cosine=scratch.basis[size_t((h+harmonics)*maximumSamples+n)]+scratch.mean[size_t(h+harmonics)]*scratch.weight[size_t(h+harmonics)];
                derivative+=(n-(count-1)*.5)*(scratch.omega[size_t(h)]/scratch.frequency)
                    *(scratch.coefficient[size_t(h)]*cosine-scratch.coefficient[size_t(h+harmonics)]*sine);
            }
            const double error=input[n]-modelValue(scratch,n);numerator+=error*derivative;denominator+=derivative*derivative;
        }
        if(denominator<1e-18)return m.frequency;
        const double change=std::clamp(numerator/denominator,-m.frequency*.025,m.frequency*.025);
        return std::clamp(m.frequency+change,hz(27.5),hz(100.5));
    }
};

PolyphonicTracker::PolyphonicTracker()=default;
PolyphonicTracker::~PolyphonicTracker()=default;
PolyphonicTracker::PolyphonicTracker(PolyphonicTracker&&) noexcept=default;
PolyphonicTracker& PolyphonicTracker::operator=(PolyphonicTracker&&) noexcept=default;
void PolyphonicTracker::prepare(double rate){
    if(!std::isfinite(rate)||rate<8000||rate>48000)throw std::invalid_argument("PolyphonicTracker expects an8..48kHz analysis stream");
    auto next=std::make_unique<Data>();next->rate=rate;next->legacy.prepare(rate,512);next->legacyShort.prepare(rate,256);next->periodicity.prepare(rate,28,100);
    for(int n=0;n<fftSize;++n){int value=n,reversed=0;for(int b=0;b<11;++b){reversed=(reversed<<1)|(value&1);value>>=1;}next->reverse[size_t(n)]=reversed;}
    for(int n=0;n<fftSize/2;++n){const double angle=-2*pi*n/fftSize;next->twiddle[size_t(n)]={float(std::cos(angle)),float(std::sin(angle))};}
    for(int group=0;group<16;++group){const int count=(group+1)*32;for(int n=0;n<count;++n)next->windows[size_t(group)][size_t(n)]=float(.5-.5*std::cos(2*pi*n/(count-1)));}
    data=std::move(next);reset();
}
void PolyphonicTracker::reset() noexcept {if(data){data->states={};data->havePrevious=false;data->lastCount=0;data->freshSamples=data->quietSamples=data->admissionSuppressed=0;data->lastRecentPower=0;data->confirmations={};data->hintCount=data->hintWrite=0;}}
void PolyphonicTracker::analyse(const float* wave,int count,int hop,const Settings& settings,Frame& output) noexcept {
    output={};if(!data||!wave||count<128||hop<1)return;
    auto& d=*data;const int supplied=count;count=std::min(count,maximumSamples);count-=count%32;
    const double threshold=std::pow(10.,std::clamp(double(settings.newNoteThresholdDb),-100.,-12.)/20);
    const double noise=std::isfinite(settings.noiseRms)?std::max(0.f,settings.noiseRms):0;
    bool anyActive=false;for(const auto& state:d.states)anyActive|=state.view.active;
    double recentEnergy=0;const int recent=std::min(hop,count);
    for(int n=supplied-recent;n<supplied;++n){const double x=std::isfinite(wave[n])?wave[n]:0;recentEnergy+=x*x;}
    if(std::sqrt(recentEnergy/std::max(1,recent))>std::max(threshold,noise*2.5)){d.freshSamples=std::min(4096,d.freshSamples+hop);d.quietSamples=0;}
    else {d.quietSamples+=hop;if(d.quietSamples>d.rate*.025)d.freshSamples=0;}
    if(!anyActive){count=std::min(count,d.freshSamples);count-=count%32;if(count<128)return;}
    wave+=supplied-count;
    const int polyphony=std::clamp(settings.polyphony,1,maximumVoices);double mean=0;
    for(int n=0;n<count;++n){d.wave[size_t(n)]=std::isfinite(wave[n])?std::clamp(wave[n],-16.f,16.f):0;mean+=d.wave[size_t(n)];}
    mean/=count;d.energy=0;for(int n=0;n<count;++n){d.wave[size_t(n)]-=float(mean);d.energy+=double(d.wave[size_t(n)])*d.wave[size_t(n)];}
    double recentSum=0,recentSquared=0;const int onsetWindow=std::min(count,64);
    for(int n=count-onsetWindow;n<count;++n){const double x=d.wave[size_t(n)];recentSum+=x;recentSquared+=x*x;}
    const double recentPower=std::max(0.,recentSquared-recentSum*recentSum/onsetWindow)/onsetWindow;
    if(recentPower<d.lastRecentPower*.35)d.admissionSuppressed=512;
    else if(recentPower>d.lastRecentPower*1.6&&recentPower>threshold*threshold*4)d.admissionSuppressed=0;
    d.lastRecentPower=recentPower;d.admissionSuppressed=std::max(0,d.admissionSuppressed-hop);
    const bool admitNew=d.admissionSuppressed==0&&recentPower>std::max(threshold*threshold,noise*noise*6.25);
    output.rms=float(std::sqrt(d.energy/count));output.observationSamples=count;
    d.spectrum(count,hop);d.candidateCount=0;
    const auto hint=d.periodicity.estimate(d.wave.data(),count,.08f,true);
    bool stableHint=false;
    if(hint.valid&&hint.confidence>.90){d.hintHistory[size_t(d.hintWrite)]=hint.pitch;d.hintWrite=(d.hintWrite+1)%4;d.hintCount=std::min(4,d.hintCount+1);float lo=hint.pitch,hi=hint.pitch;for(int h=0;h<d.hintCount;++h){lo=std::min(lo,d.hintHistory[size_t(h)]);hi=std::max(hi,d.hintHistory[size_t(h)]);}stableHint=d.hintCount>=3&&hi-lo<=.10;}else d.hintCount=d.hintWrite=0;
    double totalSpectrum=0,logs=0,maximumSpectrum=0;int bins=0;
    for(int k=std::max(2,int(50*fftSize/d.rate));k<int(4500*fftSize/d.rate)&&k<fftSize/2;++k){const double value=d.powers[size_t(k)];totalSpectrum+=value;logs+=std::log(std::max(value,1e-30));maximumSpectrum=std::max(maximumSpectrum,value);++bins;}
    const double flatness=std::exp(logs/std::max(1,bins))/(totalSpectrum/std::max(1,bins)+1e-30);
    const double crest=maximumSpectrum/(totalSpectrum/std::max(1,bins)+1e-30);
    const bool pitchedSpectrum=(flatness<=.12||(hint.valid&&hint.confidence>.90))&&crest>=6;
    if(admitNew&&pitchedSpectrum&&count<512&&settings.earlyAcquisition&&hint.valid&&hint.confidence>.97) d.addCandidate(hint.frequency,count);
    if(admitNew&&pitchedSpectrum&&settings.legacySeeds&&(count==512||(settings.earlyAcquisition&&count>=256))){
        TimeDomainPursuit::SpectralEvidence evidence;evidence.maximumHz=float(d.rate*.45);evidence.resolutionHz=float(d.rate/count);evidence.sortedPeaks=true;
        for(int n=0;n<73;++n)evidence.frequencies[size_t(n)]=float(hz(n+28));
        for(const auto& state:d.states)if(state.view.active){const int note=int(std::lround(state.view.pitch))-28;if(note>=0&&note<73)evidence.active[size_t(note)]=true;}
        for(int p=0;p<d.peakCount;++p){const auto& peak=d.peaks[size_t(p)];evidence.peaks[size_t(p)]={float(peak.frequency),float(std::sqrt(peak.power)),0};evidence.maximumMagnitude=std::max(evidence.maximumMagnitude,float(std::sqrt(peak.power)));}evidence.count=d.peakCount;
        if(stableHint){
            const auto physical=d.assess(hint.frequency,count);
            const auto lower=d.assess(hint.frequency*.5,count);
            const int fifth=d.nearest(hint.frequency*5,hint.frequency*5*.015),seventh=d.nearest(hint.frequency*7,hint.frequency*7*.015);
            const bool primitive=fifth>=0&&seventh>=0&&d.peaks[size_t(fifth)].power>maximumSpectrum*.000064&&d.peaks[size_t(seventh)].power>maximumSpectrum*.000064;
            if(((physical.root&&physical.support>=2)||primitive)&&(count==512||!lower.root||lower.support<3))evidence.anchorNote=hint.note-28;
        }
        auto& seed=count==512?d.legacy:d.legacyShort;
        seed.analyse(d.wave.data(),count,d.legacyStrength,d.legacyConfidence,polyphony,settings.sensitivity,&evidence);
        for(int n=0;n<73;++n){if(d.legacyStrength[size_t(n)]>threshold*.8)d.confirmations[size_t(n)]=std::min(100,d.confirmations[size_t(n)]+1);else d.confirmations[size_t(n)]=0;}
        for(int n=0;n<73;++n)if(d.legacyStrength[size_t(n)]>threshold*.8&&d.confirmations[size_t(n)]>=(stableHint&&hint.note==n+28?6:10)){
            const double f=hz(n+28);const auto physical=d.assess(f,count);
            if(count<512&&!(hint.valid&&hint.confidence>.97&&hint.note==n+28))continue;
            if((!physical.root||physical.support<3)&&!(stableHint&&hint.note==n+28&&hint.confidence>.97))continue;
            if(count<512){const auto lower=d.assess(f*.5,count);if(lower.root&&lower.support>=3&&lower.score>physical.score*.30)continue;}
            d.addCandidate(f,count,true);
            for(int c=0;c<d.candidateCount;++c)if(std::abs(pitch(d.candidates[size_t(c)].frequency)-(n+28))<.65){
                d.candidates[size_t(c)].frequency=f;
                d.candidates[size_t(c)].score=d.legacyStrength[size_t(n)]*d.legacyStrength[size_t(n)];
            }
        }
    }
    std::sort(d.candidates.begin(),d.candidates.begin()+d.candidateCount,[](const auto& a,const auto& b){return a.score>b.score;});
    int used=0;std::array<int,maximumVoices> previousSlots{};previousSlots.fill(-1);
    // Established sources enter the joint model before any new proposal.
    for(int slot=0;slot<maximumVoices&&used<polyphony;++slot)if(d.states[size_t(slot)].view.active){
        auto& state=d.states[size_t(slot)];double frequency=state.view.frequency;
        d.models[size_t(used)].prepare(frequency,d.rate,count);previousSlots[size_t(used++)]=slot;
    }
    double residual=d.energy;
    for(int c=0;c<d.candidateCount&&used<polyphony;++c){
        const auto& candidate=d.candidates[size_t(c)];bool duplicate=false;
        for(int j=0;j<used;++j)duplicate|=std::abs(pitch(d.models[size_t(j)].frequency)-pitch(candidate.frequency))<.65;
        if(duplicate||(!candidate.legacy&&(candidate.support<4||used>0||count<192)))continue;
        if(anyActive){
            int independent=0;bool independentRoot=false;
            for(int h=1;h<=6&&candidate.frequency*h<d.rate*.45;++h){
                bool shared=false;for(int k=0;k<used;++k)for(int g=1;g<=harmonics;++g)
                    if(std::abs(candidate.frequency*h-d.models[size_t(k)].frequency*g)<d.rate/count*.65)shared=true;
                if(!shared&&d.nearest(candidate.frequency*h,std::max(d.rate/count*.18,candidate.frequency*h*.022))>=0){++independent;if(h==1)independentRoot=true;}
            }
            // A harmonic of a held string is not a newly articulated source.
            // Octave-unison reattacks require independent residual excitation
            // evidence; this conservative prototype does not infer them merely
            // because a semitone dictionary can explain the existing overtones.
            if(independent<2||(!independentRoot&&independent<3))continue;
        }
        d.models[size_t(used)].prepare(candidate.frequency,d.rate,count);
        if(candidate.legacy)++used;
        else {
            d.fit(d.models.data(),used+1,d.wave.data(),count);const double after=d.residualFor(used+1,count);
            const double improvement=(d.energy-after)/std::max(d.energy,1e-20);
            double total=0;for(int h=0;h<harmonics;++h){const double a=amplitude(d.models[size_t(used)],h);total+=a*a;}
            const double root=amplitude(d.models[size_t(used)],0),level=std::sqrt(total*.5);
            if(improvement>.85&&level>std::max(threshold,noise*4)&&root*root>total*.035)++used;
        }
    }
    if(used>0){
        d.fit(d.models.data(),used,d.wave.data(),count);
        // Jointly separate sources using the full past-only frame, then
        // measure current phase on only the most recent three periods. Root
        // and upper-partial phase velocities must agree before changing F0.
        // Unconstrained short-window residual gradients drifted even on a
        // stationary triad; they are deliberately not used as a pitch update.
        for(int j=0;j<used;++j){
            auto& model=d.models[size_t(j)];auto& observation=d.observations[size_t(j)];observation={};
            const int recent=std::min(count,std::max(192,int(std::ceil(d.rate/model.frequency*3.0))));
            observation.count=recent;observation.frequency=model.frequency;
            for(int n=0;n<recent;++n){const int sample=count-recent+n;double value=d.wave[size_t(sample)];for(int k=0;k<used;++k)if(k!=j)value-=d.modelValue(d.models[size_t(k)],sample);d.isolated[size_t(n)]=float(value);}
            d.scratch.prepare(model.frequency,d.rate,recent);
            if(!d.fit(&d.scratch,1,d.isolated.data(),recent))continue;
            double signalPower=0,errorPower=0,partialPower=0;
            for(int n=0;n<recent;++n){const double x=d.isolated[size_t(n)],e=x-d.modelValue(d.scratch,n);signalPower+=x*x;errorPower+=e*e;}
            for(int h=0;h<harmonics;++h){const double a=amplitude(d.scratch,h);partialPower+=a*a;}
            observation.confidence=std::clamp(1-errorPower/std::max(signalPower,1e-20),0.,1.);
            struct PhaseEstimate {double frequency=0,weight=0,error=0;};std::array<PhaseEstimate,harmonics> estimates{};int estimatesCount=0;
            const int slot=previousSlots[size_t(j)];const auto* state=slot>=0?&d.states[size_t(slot)]:nullptr;
            for(int h=0;h<harmonics;++h){
                const double a=d.scratch.coefficient[size_t(h)],b=d.scratch.coefficient[size_t(h+harmonics)];
                const double angle=d.scratch.omega[size_t(h)]*(recent-1),cs=std::cos(angle),sn=std::sin(angle);
                const double re=b*cs+a*sn,im=b*sn-a*cs;
                observation.phase[size_t(2*h)]=re;observation.phase[size_t(2*h+1)]=im;
                bool shared=false;for(int k=0;k<used;++k)if(k!=j)for(int g=0;g<harmonics;++g)
                    if(std::abs(model.omega[size_t(h)]-d.models[size_t(k)].omega[size_t(g)])*count<2*pi*.65)shared=true;
                if(!state||state->phaseCount<1||shared||h>4||amplitude(d.scratch,h)*amplitude(d.scratch,h)<partialPower*.0064)continue;
                const double pr=state->phaseEnd[size_t(2*h)],pii=state->phaseEnd[size_t(2*h+1)];
                const double magnitude=std::sqrt((re*re+im*im)*(pr*pr+pii*pii));
                const double effectiveHop=hop-(recent-state->phaseCount)*.5;
                if(magnitude<1e-12||effectiveHop<hop*.25)continue;
                const double omega=2*pi*model.frequency*(h+1)/d.rate,previousOmega=2*pi*state->phaseFrequency*(h+1)/d.rate;
                const double shift=(omega*(recent-1)-previousOmega*(state->phaseCount-1))*.5;
                const double advance=omega*effectiveHop+shift;
                const double phase=std::atan2(im*pr-re*pii,re*pr+im*pii);
                const double error=std::remainder(phase-advance,2*pi);
                const double frequency=model.frequency+error*d.rate/(2*pi*(h+1)*effectiveHop);
                if(frequency>0&&std::abs(pitch(frequency)-pitch(model.frequency))<.75)
                    estimates[size_t(estimatesCount++)]={frequency,magnitude/(h+1),error};
            }
            if(estimatesCount>=2&&observation.confidence>.65){
                std::sort(estimates.begin(),estimates.begin()+estimatesCount,[](const auto&a,const auto&b){return a.frequency<b.frequency;});
                double sumWeight=0;for(int e=0;e<estimatesCount;++e)sumWeight+=estimates[size_t(e)].weight;
                double cumulative=0,median=estimates[0].frequency;for(int e=0;e<estimatesCount;++e){cumulative+=estimates[size_t(e)].weight;if(cumulative>=sumWeight*.5){median=estimates[size_t(e)].frequency;break;}}
                double frequencySum=0,weightSum=0,innovation=0;int agreeing=0;
                for(int e=0;e<estimatesCount;++e){const auto& estimate=estimates[size_t(e)];if(std::abs(pitch(estimate.frequency)-pitch(median))>.16)continue;frequencySum+=estimate.frequency*estimate.weight;weightSum+=estimate.weight;innovation+=estimate.error*estimate.error*estimate.weight;++agreeing;}
                if(agreeing>=2&&weightSum>sumWeight*.55){
                    const double measured=frequencySum/weightSum,change=std::clamp(pitch(measured)-pitch(model.frequency),-.20,.20);
                    observation.frequency=hz(pitch(model.frequency)+(std::abs(change)<.015?0:change*.45));
                    observation.innovation=std::clamp(innovation/weightSum,0.,1.);
                }
            }
        }
        residual=d.residualFor(used,count);
    }
    output.explained=float(std::clamp(1-residual/std::max(d.energy,1e-20),0.,1.));
    std::array<bool,maximumVoices> occupied{};
    for(int j=0;j<used;++j){
        auto& m=d.models[size_t(j)];double power=0;for(int h=0;h<harmonics;++h){const double a=amplitude(m,h);power+=a*a;}
        const double level=std::sqrt(power*.5),root=amplitude(m,0),p=pitch(m.frequency);
        int slot=previousSlots[size_t(j)];
        const bool existing=slot>=0;
        const bool missingRoot=!existing&&used==1&&stableHint&&hint.note==int(std::lround(p))&&hint.confidence>.97;
        const bool supported=pitchedSpectrum&&level>std::max(threshold*(existing?.08:1.0),noise*(existing?1.25:2.5))
            &&(root*root>power*(existing?.001:.01)||missingRoot)&&std::isfinite(p);
        if(!supported)continue;
        if(slot<0){for(int s=0;s<maximumVoices;++s)if(!d.states[size_t(s)].view.active&&!occupied[size_t(s)]){slot=s;break;}}
        if(slot<0)continue;
        occupied[size_t(slot)]=true;auto& state=d.states[size_t(slot)];const bool onset=!state.view.active;
        const auto& observation=d.observations[size_t(j)];
        state.phaseEnd=observation.phase;state.phaseCount=observation.count;state.phaseFrequency=m.frequency;
        const float innovation=float(observation.innovation);
        state.absent=0;state.view={true,onset,std::uint32_t(slot),onset?int(std::lround(p)):state.view.anchorNote,
            onset?0:state.view.ageSamples+hop,float(observation.frequency),float(pitch(observation.frequency)),float(level),float(observation.confidence),innovation,
            observation.count};
        for(int h=0;h<harmonics;++h)state.profile[size_t(h)]=amplitude(m,h);
    }
    const int release=int(d.rate*std::clamp(double(settings.releaseMs),5.,300.)*.001);
    for(int slot=0;slot<maximumVoices;++slot){auto& state=d.states[size_t(slot)];
        if(!occupied[size_t(slot)]&&state.view.active){state.view.onset=false;state.absent+=hop;if(state.absent>=release)state={};}
        output.voices[size_t(slot)]=state.view;if(state.view.active)++output.count;
    }
}
}
