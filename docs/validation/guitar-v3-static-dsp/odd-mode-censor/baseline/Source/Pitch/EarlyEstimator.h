// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <vector>

namespace flysnapper::pitch {
// Research-only short-observation candidate. This is deliberately independent
// of PitchDetector: short-window harmonic regression can fit wrong guitar
// attack hypotheses. An accepted proposal is a heuristic, not proof of pitch
// or a latency guarantee. It is not enabled by the shipping detector.
class EarlyEstimator {
public:
    static constexpr int lowNote=36,highNote=88,maxSamples=512,maxHarmonics=6;
    struct Result {
        int bestNote=-1;             // best model even if no note is accepted
        std::array<int,6> notes{{-1,-1,-1,-1,-1,-1}};
        int count=0;
        float confidence=0,fit=0,margin=0,fundamentalFraction=0,rms=0;
        bool accepted=false;
    };
    // Tables are computed here, outside realtime processing. One prepared
    // observation length is used; 192/256/384/512 samples correspond to
    // 4/5.33/8/10.67ms at48kHz, before any hardware/host delay.
    void prepare(double sampleRate,int observationSamples=384);
    // Uses only the most recent observationSamples available. maxVoices>1
    // explicitly abstains: a single harmonic model cannot establish polyphony.
    // Input may be finite or nonfinite; nonfinite samples become zero.
    Result estimate(const float* input,int samples,int maxVoices=1) const noexcept;
    int observationSamples() const noexcept {return length;}
    double sampleRate() const noexcept {return rate;}
private:
    static constexpr int columns=2*maxHarmonics,noteCount=highNote-lowNote+1;
    struct Model {
        std::array<float,columns*maxSamples> basis{};
        std::array<double,columns*columns> inverse{};
        std::array<float,columns> weights{};
    };
    std::vector<Model> models; // resized only by prepare()
    double rate=48000;
    int length=0;
};
}
