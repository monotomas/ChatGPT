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
    };
    void prepare(double analysisRate,int lowestNote=28,int highestNote=100);
    // Uses the latest available samples, capped at1024. The difference
    // comparison length is constant across all tested lags (half the frame),
    // so a candidate needs two observed periods at the bottom of that frame's
    // frequency range. The MIDI range is additionally bounded by prepare().
    Result estimate(const float* wave,int samples,float threshold=.08f,
                    bool harmonicDisambiguation=true) noexcept;
private:
    double rate=12000;
    int lowNote=28,highNote=100;
    float lowHz=41.2034f,highHz=2637.02f;
    std::array<float,maxSamples> frame{};
    std::array<float,maxSamples/2+1> difference{},normalised{};
};
}
