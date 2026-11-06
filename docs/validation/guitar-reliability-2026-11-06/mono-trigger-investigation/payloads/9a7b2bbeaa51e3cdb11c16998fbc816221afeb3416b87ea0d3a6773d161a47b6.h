// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "TimeDomainPursuit.h"
#include "YinEstimator.h"
#include "ArticulationTracker.h"
#include "NoiseConditioner.h"
#include "PolyphonicTracker.h"
#include "InputDynamics.h"

namespace flysnapper::pitch {
struct Settings {
    bool enabled=true;
    bool polyphonic=false;
    float thresholdDb=-55.0f;
    float sensitivity=0.65f; // 0..1; higher detects weaker notes
    float releaseMs=35.0f;
    int polyphony=6;
    bool monoBend=false;
    float bendRangeSemitones=2.0f;
    bool polyBend=false;
    bool autoInput=true;
    // Input dynamics may close fresh acquisition before its release gain has
    // reached silence. Existing sources retain their independent continuation
    // and release rules. Changing this admission flag never resets a source.
    bool allowNewNotes=true;
    InputDynamics::Settings inputDynamics{};
    // Calibration observes the input continuously without analysing or
    // emitting notes. Its known playing phase protects the noise model.
    bool observeOnly=false;
    bool protectNoiseLearning=false;
};
struct Event {bool on=false;int note=0,velocity=0,offset=0;bool pitchChange=false;float pitch=0;int voiceId=-1;};
struct Output {
    static constexpr size_t capacity=512;
    std::array<Event,capacity> events{};
    size_t size=0;
    bool overflow=false;
    void clear() noexcept {size=0;overflow=false;}
};
struct Status {
    float levelDb=-120.0f;
    float confidence=0.0f;
    int activeNotes=0;
    int strongestNote=-1;
    bool inputPresent=false;
    uint16_t activePitchClasses=0;
    float continuousPitch=-1.0f;
};
// Causal, bounded detector. prepare() computes tables outside the audio thread.
// process() appends sample-relative MIDI decisions; it never accesses GUI/files,
// waits for future audio, locks or allocates. Audio samples must be mono finite
// floats; non-finite input is treated as silence. Events always use A4=440Hz.
class PitchDetector {
public:
    static constexpr int lowNote=28,highNote=100,noteCount=highNote-lowNote+1;
    PitchDetector();
    void prepare(double sampleRate);
    void configure(const Settings&) noexcept;
    void reset(Output&,int offset=0) noexcept;
    void resetTrackingPreservingInput(Output&,int offset=0) noexcept;
    void process(const float* mono,int samples,Output&) noexcept;
    Status status() const noexcept {return view;}
    // Audio-thread hint for an upstream stationary-noise observer. Pending
    // credible string evidence protects the onset before discrete Note On;
    // an input gate opening by itself is not musical protection.
    bool backgroundProtected() const noexcept {return view.activeNotes>0||candidateProtectionRemaining>0;}
    InputDynamics::Meter inputDynamicsMeter() const noexcept {return analysisDynamics.meter();}
    double sampleRate() const noexcept {return rate;}
public:
    // Details are deliberately fixed-size; no processing-time containers grow.
    static constexpr int fftSize=2048,windowSize=1024,hopSize=32;
    struct Complex {float real=0,imag=0;};
    struct Voice {bool active=false;float strength=0;int absent=0,age=0,id=-1;};
    Settings config;
    Status view;
    double rate=48000.0,analysisRate=12000.0;
    double analysisPhase=0,analysisStep=.25;
    float previousFiltered=0;
    std::array<float,windowSize> ring{},rawReferenceRing{};
    std::array<float,512> window{};
    std::array<Complex,fftSize> transform{};
    std::array<Complex,fftSize/2> twiddle{};
    std::array<int,fftSize> bitReverse{};
    std::array<float,fftSize/2+1> spectrum{};
    TimeDomainPursuit pursuit;
    YinEstimator monoEstimator;
    ArticulationTracker monoArticulation;
    PartialPhaseTracker monoPartialPhase;
    NoiseConditioner noiseConditioner;
    InputDynamics analysisDynamics;
    PolyphonicTracker polyTracker;
    struct PublishedPolyVoice {int anchor=-1;float pitch=-1;};
    std::array<PublishedPolyVoice,PolyphonicTracker::maximumVoices> publishedPoly{};
    std::array<ArticulationTracker,PolyphonicTracker::maximumVoices> polyArticulation{};
    bool polyShadowMode=false;
    int64_t analysisClock=0;
    int monoArticulatedNote=-1;
    int candidateProtectionRemaining=0;
    double preparedPursuitRate=0;
    std::array<float,noteCount> frequency{},scores{},strength{},retriggerBaseline{};
    std::array<int,noteCount> confirmations{};
    std::array<bool,noteCount> candidateAdmissible{};
    std::array<Voice,noteCount> voices{};
    int writeIndex=0,filled=0,sinceAnalysis=0,signalSamples=0,silentSamples=0;
    float dcX=0,dcY=0,low1=0,low2=0,low3=0,low4=0,envelope=0,fastEnvelope=0,slowEnvelope=0;
    float gate=0.001778f,lowpass=0,releaseCoefficient=0,attackCoefficient=0,fastCoefficient=0,slowCoefficient=0;
    float cleanEnvelope=0,rawCleanEnvelope=0,cleanEnvelopeCoefficient=0;
    int releaseSamples=420;
    float currentPitch=-1,previousPitch=-1,dcPole=.998f,previousAttackRatio=0,measuredPeriod=0;
    int measuredPeriodNote=-1;
    int attackCooldown=0,attackRemaining=0;
    float innovationEnvelope=0,previousInnovationRatio=0,innovationCoefficient=0,onsetEnvelope=0,onsetCoefficient=0;
    bool phaseAttack=false;
    int phaseAttackAge=0,pitchHistoryCount=0,pitchHistoryWrite=0;
    std::array<float,4> pitchHistory{};
    float pitchHistoryRange=0;
    float lastSentPitch=-1,phaseAttackOriginPitch=-1;
    int lastPitchAnchor=-1;
    bool monoTrackedOnly=false;
    bool monoPhysicalStable=false;
    int polyAdmission=0,polyAnchor=-1;
    int pendingPolyRepick=-1,pendingPolyRepickAge=0;
    float pendingPhasePeriod=0;
    // Two complex pre-arm observations for each odd harmonic 1/3/5/7.
    std::array<double,16> pendingPhaseCoefficients{};
    std::array<float,fftSize/2+1> spectralBaseline{};
    float spectralUpdate=0;
    std::array<bool,noteCount> independentNovelty{};
    std::array<float,hopSize> polyPrediction{};
    bool predictionValid=false;
    int predictionIndex=hopSize,predictionAge=0,predictionCooldown=0;
    float predictionFast=0,predictionSlow=0,predictionObserved=0,predictionExpected=0;
    float predictionFastCoefficient=0,predictionSlowCoefficient=0;
    bool auditReferenceAdopted=false;float auditReferenceConfidence=0;
    bool configured=false,needsReset=false;
    bool canAdmit() const noexcept {return config.allowNewNotes&&analysisDynamics.allowsNewNotes();}
    void fft(bool inverse=false) noexcept;
    void analyse(int offset,Output&) noexcept;
    void analyseMono(int count,int offset,Output&) noexcept;
    void analysePoly(int count,int offset,Output&) noexcept;
    void updateVoices(int offset,Output&) noexcept;
    void updatePolyExpression(int offset,Output&) noexcept;
    void emitPoly(int voice,bool on,int note,int velocity,float pitch,bool pitchChange,int offset,Output&) noexcept;
    void emit(bool on,int note,int velocity,int offset,Output&) noexcept;
    void emitPitch(int note,float pitch,int offset,Output&) noexcept;
};
}
