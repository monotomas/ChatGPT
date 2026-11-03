// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "TimeDomainPursuit.h"

namespace flysnapper::pitch {
struct Settings {
    bool enabled=true;
    bool polyphonic=false;
    float thresholdDb=-55.0f;
    float sensitivity=0.65f; // 0..1; higher detects weaker notes
    float releaseMs=35.0f;
    int polyphony=6;
};
struct Event {bool on=false;int note=0,velocity=0,offset=0;};
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
    void process(const float* mono,int samples,Output&) noexcept;
    Status status() const noexcept {return view;}
    double sampleRate() const noexcept {return rate;}
private:
    // Details are deliberately fixed-size; no processing-time containers grow.
    static constexpr int fftSize=2048,windowSize=1024,hopSize=32;
    struct Complex {float real=0,imag=0;};
    struct Voice {bool active=false;float strength=0;int absent=0,age=0;};
    Settings config;
    Status view;
    double rate=48000.0,analysisRate=12000.0;
    int decimation=4,decimationPhase=0;
    std::array<float,windowSize> ring{};
    std::array<float,windowSize> window{},prefix{};
    std::array<Complex,fftSize> transform{};
    std::array<Complex,fftSize/2> twiddle{};
    std::array<int,fftSize> bitReverse{};
    std::array<float,fftSize/2+1> spectrum{},residual{};
    TimeDomainPursuit pursuit;
    double preparedPursuitRate=0;
    std::array<float,noteCount> frequency{},scores{},strength{},previousStrength{},retriggerBaseline{};
    std::array<int,noteCount> confirmations{};
    std::array<Voice,noteCount> voices{};
    int writeIndex=0,filled=0,sinceAnalysis=0,signalSamples=0,silentSamples=0;
    float dcX=0,dcY=0,low1=0,low2=0,low3=0,low4=0,envelope=0,fastEnvelope=0,slowEnvelope=0;
    float gate=0.001778f,lowpass=0,releaseCoefficient=0,attackCoefficient=0,fastCoefficient=0,slowCoefficient=0;
    int releaseSamples=420;
    float currentPitch=-1,previousPitch=-1,dcPole=.998f,previousAttackRatio=0;
    int attackCooldown=0,attackRemaining=0;
    bool configured=false,needsReset=false;
    void fft(bool inverse=false) noexcept;
    void analyse(int offset,Output&) noexcept;
    void analyseMono(int count,int offset,Output&) noexcept;
    void analysePoly(int count,int offset,Output&) noexcept;
    void updateVoices(int offset,Output&) noexcept;
    void emit(bool on,int note,int velocity,int offset,Output&) noexcept;
};
}
