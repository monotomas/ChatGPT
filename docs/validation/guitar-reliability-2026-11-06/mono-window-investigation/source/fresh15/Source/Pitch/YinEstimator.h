// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>

namespace flysnapper::pitch {
// Independent causal YIN-CMNDF candidate. The caller owns onset/release and
// confirms decisions across frames. No frame uses future audio, and no call
// allocates, locks or accesses the GUI. Observation time is still real latency.
class YinEstimator {
public:
    static constexpr int maxSamples=1024;
    struct Result {
        bool valid=false;
        int note=-1,harmonicDivisor=1;
        float frequency=0,pitch=0,confidence=0,periodicity=0,harmonicPurity=1,rms=0;
        int observedSamples=0;
    };
    void prepare(double analysisRate,int lowestNote=28,int highestNote=100);
    // Uses the latest available samples, capped at1024. The difference
    // integer-lag comparison length is constant (half the frame),
    // so a candidate needs two observed periods at the bottom of that frame's
    // frequency range. Short-lag cubic refinement clips and normalises its
    // comparison at the available frame boundary. The MIDI range is bounded
    // by prepare(), including detuning within each endpoint's rounding bin.
    Result estimate(const float* wave,int samples,float threshold=.08f,
                    bool harmonicDisambiguation=true,bool shortOverlap=false) noexcept;
    // Follow an already observed oscillator in a narrow frequency interval.
    // Correlation is normalised independently for the delayed and reference
    // segments, so a changing decay envelope does not resemble pitch loss.
    // This is tracking evidence, never a global note-acquisition decision.
    Result track(const float* wave,int samples,float expectedPitch,
                 float searchSemitones=.8f,float minimumCorrelation=.9f) noexcept;
private:
    double rate=12000;
    int lowNote=28,highNote=100;
    float lowHz=41.2034f,highHz=2637.02f;
    std::array<float,maxSamples> frame{};
    std::array<float,maxSamples+1> difference{},normalised{};
};
}
