// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>

namespace flysnapper::pitch {
// Causal input evidence and stationary-line cancellation. This is deliberately
// not a broadband gate: an acquired, decaying string is never muted by it.
// prepare/reset are the only lifecycle operations; all storage is bounded and
// processSample/assess allocate no memory, perform no IO and acquire no locks.
class NoiseConditioner {
public:
    static constexpr int maxFrame=1024, harmonics=16, lines=2*harmonics;
    struct Evidence {
        float rms=0, backgroundRms=0, humRms=0, residualRms=0;
        float harmonicFraction=0, phaseCoherence=0, snrDb=-120;
        // A harmonic model's unexplained energy includes legitimate string
        // inharmonicity, body resonances and timbre changes. Keep that ratio
        // separate from the level above the independently learned idle floor.
        // snrDb retains the legacy conservative ratio using the larger of
        // unexplained model energy and learned idle noise; harmonicSnrDb uses
        // ONLY the fit residual, so a stale floor cannot censor trusted decay.
        float harmonicSnrDb=-120, backgroundSnrDb=-120;
        float mainsFraction=0, mainsPeriodicity=0;
        int coherentPartials=0;
        bool learnedBackground=false;
        // mainsExplained remains available for rechecking a previously
        // mis-admitted source; mainsOnly is the new-note admission flag.
        bool mainsCandidate=false, mainsExplained=false, mainsOnly=false;
        bool clearlyUnpitched=false;
    };

    explicit NoiseConditioner(double sampleRate=12000);
    void prepare(double sampleRate);
    void reset() noexcept;
    // Call at the analysis sample rate, before pitch estimation. Protection
    // freezes adaptation, not subtraction: only the previously learned line
    // model is removed. A new excitation also freezes adaptation immediately.
    float processSample(float sample,bool protectPitchedSignal=false,
                        bool musicalExcitation=false) noexcept;
    // Inspect a trailing CLEANED frame. Candidate frequency/confidence belong
    // to the caller's pitch estimator. Metrics are gain-relative; no absolute
    // input level is used to label a frame as a wanted musical note.
    // mainsOnly is evidence, not an unconditional instruction to mute audio.
    // A held/bent note or independently identified musical excitation overrides
    // startup mains admission suspicion. An exactly identical intended 50-Hz
    // signal and 50-Hz interference cannot be distinguished from audio alone.
    Evidence assess(const float* frame,int samples,float candidateHz,
                    float candidateConfidence=0,bool held=false,
                    bool musicalExcitation=false) const noexcept;
    float backgroundRms() const noexcept;
    float learnedHumRms() const noexcept;
    double sampleRate() const noexcept {return rate;}
    double mainsFrequency(int family) const noexcept;

private:
    struct Line {double c=1,s=0,dc=1,ds=0,a=0,b=0,previousA=0,previousB=0;};
    std::array<Line,lines> oscillators{};
    std::array<double,2> familyFrequency{{50,60}};
    double rate=12000,adapt=0,fastCoefficient=0,slowCoefficient=0,noiseRise=0,noiseFall=0;
    double fastPower=0,slowPower=0,residualFast=0,residualSlow=0,noisePower=0,inputNoisePower=0,humPower=0;
    int freezeSamples=0,attackCooldown=0,idleSamples=0,frequencySamples=0,renormalise=0;
    uint64_t sampleClock=0,frequencySnapshot=0;
    void updateFrequency() noexcept;
    void setOscillatorSteps() noexcept;
};
}
