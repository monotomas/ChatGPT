// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <memory>

namespace flysnapper::pitch {
// Causal free-phase harmonic group pursuit. Unlike a Gaussian comb, candidate
// fundamentals do not require a separated FFT maximum: upper partials and their
// time-domain phase jointly support notes with unresolved fundamentals.
// Observation time is samples/analysisRate; no end-to-end latency is implied.
class TimeDomainPursuit {
public:
    static constexpr int lowNote=28,highNote=100,noteCount=73,maxSamples=512;
    TimeDomainPursuit();
    ~TimeDomainPursuit();
    TimeDomainPursuit(TimeDomainPursuit&&) noexcept;
    TimeDomainPursuit& operator=(TimeDomainPursuit&&) noexcept;
    TimeDomainPursuit(const TimeDomainPursuit&)=delete;
    TimeDomainPursuit& operator=(const TimeDomainPursuit&)=delete;
    void prepare(double analysisRate,int samples=512);
    // Input oldest-to-newest. Uses prepared trailing observation length. Output
    // arrays are always overwritten. No allocation, factorisation, locks or IO.
    // A model with no meaningful fundamental coefficient is rejected to avoid
    // hallucinated subharmonics. This deliberately cannot guarantee separation
    // of octave-related voices or a wholly missing fundamental.
    void analyse(const float* wave,int count,std::array<float,noteCount>& strength,
                 std::array<float,noteCount>& confidence,int maxVoices=6,float sensitivity=.65f) noexcept;
private:
    struct Data;
    std::unique_ptr<Data> data;
};
}
