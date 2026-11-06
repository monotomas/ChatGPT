// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <memory>
#include <cstdint>

namespace flysnapper::pitch {
// Causal free-phase harmonic group pursuit. Unlike a Gaussian comb, candidate
// fundamentals do not require a separated FFT maximum: upper partials and their
// time-domain phase jointly support notes with unresolved fundamentals.
// Observation time is samples/analysisRate; no end-to-end latency is implied.
class TimeDomainPursuit {
public:
    static constexpr int lowNote=28,highNote=100,noteCount=73,maxSamples=512,predictionSamples=32;
    TimeDomainPursuit();
    ~TimeDomainPursuit();
    TimeDomainPursuit(TimeDomainPursuit&&) noexcept;
    TimeDomainPursuit& operator=(TimeDomainPursuit&&) noexcept;
    TimeDomainPursuit(const TimeDomainPursuit&)=delete;
    TimeDomainPursuit& operator=(const TimeDomainPursuit&)=delete;
    void prepare(double analysisRate,int samples=512);
    bool copyPrediction(std::array<float,predictionSamples>&) const noexcept;
    struct CpuProfile {uint64_t analyseNs=0,retuneNs=0,basisNs=0,inverseNs=0,gramNs=0,analysisCalls=0,retuneCalls=0,modelUpdates=0;};
    bool copyCpuProfile(CpuProfile&) const noexcept;
    struct Peak {float frequency=0,magnitude=0,novelty=0;};
    struct SpectralEvidence {
        std::array<Peak,256> peaks{};int count=0;
        // Optional cleaned pre-dynamics frequency observations. The ordinary
        // peaks still describe processed-fit admission and novelty.
        std::array<Peak,256> frequencyPeaks{};int frequencyPeakCount=0;
        float frequencyMaximumMagnitude=0;bool independentFrequencyEvidence=false;
        std::array<float,noteCount> frequencies{};
        std::array<bool,noteCount> active{};
        float resolutionHz=23.4375f,maximumMagnitude=0,maximumHz=5400;
        // Optional stable periodic-series anchor, still subject to observed
        // fundamental or coprime upper-partial evidence in the caller.
        int anchorNote=-1;
        // True only when the supplied finite peaks are in frequency order.
        bool sortedPeaks=false;
    };
    // Requires two resolved physical partials (including the fundamental),
    // independent of harmonics already accounted for by other voices. Novel
    // evidence additionally requires an actual rise from the spectral baseline.
    static bool hasIndependentPeaks(int note,const SpectralEvidence&,
                                    const std::array<bool,noteCount>& explained,bool requireNovelty=false) noexcept;
    // Input oldest-to-newest. Uses prepared trailing observation length. Output
    // arrays are always overwritten. No allocation, locks or IO. Analysis uses a
    // bounded incremental Cholesky factorisation of at most 96 columns.
    // A model normally requires a meaningful fundamental coefficient. Strong
    // octave-envelope evidence plus independent upper partials can support a
    // shared fundamental. Neither rule guarantees arbitrary octave separation
    // or reconstruction of a wholly missing fundamental. A single held source
    // may retain a weak root through an octave-up periodicity hint only with
    // an observed root and independent odd-partial support.
    void analyse(const float* wave,int count,std::array<float,noteCount>& strength,
                 std::array<float,noteCount>& confidence,int maxVoices=6,float sensitivity=.65f,const SpectralEvidence* evidence=nullptr) noexcept;
private:
    struct Data;
    std::unique_ptr<Data> data;
};
}
