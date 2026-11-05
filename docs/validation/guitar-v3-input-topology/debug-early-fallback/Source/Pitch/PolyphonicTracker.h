// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>

namespace flysnapper::pitch {
// Causal harmonic source tracking. Voice identity belongs to a tracked source,
// not a rounded semitone. All observations are past-only; observation length
// and note-admission decisions remain real detection latency.
class PolyphonicTracker {
public:
    static constexpr int maximumVoices=6,maximumSamples=512;
    struct Settings {
        int polyphony=6;
        float sensitivity=.65f;
        float newNoteThresholdDb=-60;
        float noiseRms=0;
        float releaseMs=35;
        bool earlyAcquisition=true;
        bool legacySeeds=true;
        // Precision checkpoint: only previously confirmed discrete detector
        // NoteOns may introduce sources. Fixed-note Offs do not end bends.
        bool externalSeedMode=false;
    };
    struct Voice {
        bool active=false,onset=false;
        std::uint32_t id=0;
        int anchorNote=-1,ageSamples=0;
        float frequency=0,pitch=-1,level=0,confidence=0;
        float innovation=0;
        int observationSamples=0;
        bool retrigger=false;
        // False means current independent partials cannot establish F0 motion;
        // a retained identity is not evidence that the string stayed in tune.
        bool phaseObservable=false;
    };
    struct Frame {
        std::array<Voice,maximumVoices> voices{};
        int count=0,observationSamples=0;
        float rms=0,explained=0;
    };
    PolyphonicTracker();
    ~PolyphonicTracker();
    PolyphonicTracker(PolyphonicTracker&&) noexcept;
    PolyphonicTracker& operator=(PolyphonicTracker&&) noexcept;
    PolyphonicTracker(const PolyphonicTracker&)=delete;
    PolyphonicTracker& operator=(const PolyphonicTracker&)=delete;
    void prepare(double analysisRate,bool prepareInternalAdmissions=true);
    void reset() noexcept;
    // Fixed capacity12; false means invalid/unprepared/full. This is an
    // admission request, not an unconditional NoteOn: an existing continuous
    // source that already explains the seed retains its identity.
    bool queueAdmission(int note,bool retrigger=false) noexcept;
    // wave is oldest-to-newest. hop is the actual elapsed sample count since
    // the previous observation. count may grow during acquisition, up to512.
    // No allocations, locks, files or operating-system calls occur here.
    void analyse(const float* wave,int count,int hop,const Settings&,Frame&) noexcept;
private:
    struct Data;
    std::unique_ptr<Data> data;
};
}
