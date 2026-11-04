// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>

namespace flysnapper::pitch {
// Research observer for an ALREADY ESTABLISHED acoustic oscillator. This is
// neither a global pitch detector nor an intent classifier. Give it real
// trailing audio and the tracked source frequency, including frames where
// a global detector was distracted by another sound. No allocation or locks.
class HarmonicContinuity {
public:
    static constexpr int maximumPartials=8,maximumSamples=1024;
    struct Partial {
        float power=0,recentPower=0,baselinePower=0,phaseInnovation=0,phaseDeparture=0,phaseWeight=0;
        float frequencyOffsetHz=0,projectionSnrDb=-120;
        bool observed=false,phaseTrusted=false,continuing=false,changed=false,energyRising=false;
    };
    struct Evidence {
        std::array<Partial,maximumPartials> partials{};
        int observationSamples=0,observedPartials=0,phaseSupport=0;
        int continuingPartials=0,changedPartials=0,energyRisePartials=0;
        int departurePartials=0;
        int coherentEnergyRisePartials=0;
        float signalPower=0,foregroundPower=0,residualPower=0,harmonicFraction=0;
        float phaseContinuity=0,commonResetRadians=0,resetCoherence=0;
        float foregroundNovelty=0,residualNovelty=0,noveltyFraction=0;
        float foregroundDeltaPower=0,residualDeltaPower=0,baselineForegroundPower=0,baselineResidualPower=0;
        float commonEnergyGainDb=0,energyGainConsensus=0;
        float frequencyOffsetHz=0,frequencyConsensus=0;
        bool frequencyComparable=false;
    };
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    // lastSample is the sample index of wave[samples-1] at this analysis rate.
    // The absolute phase prediction uses each actual observation centre;
    // changing window length must not masquerade as a new excitation.
    Evidence observe(const float* wave,int samples,float trackedFrequencyHz,
                     std::int64_t lastSample) noexcept;
private:
    double rate=12000,previousCentre=0,previousFrequency=0;
    double residualBaseline=0;
    std::array<double,maximumPartials> previousPhase{},previousPower{},baselinePower{};
    std::array<double,maximumPartials> departure{},detuning{};
    std::array<double,maximumPartials> referenceAge{};
    std::array<bool,maximumPartials> previousTrusted{};
    bool initialized=false;
};
}
