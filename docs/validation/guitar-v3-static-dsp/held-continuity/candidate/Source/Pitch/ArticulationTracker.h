// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>

namespace flysnapper::pitch {
// Per-voice, causal change-point evidence. A pitch wheel is not a replacement
// for note articulation: an abrupt fretted change can create a new note without
// a rise in amplitude. Smooth trajectories, including bends ending at an exact
// semitone, retain their original note anchor. Audio alone cannot identify a
// performer's intent when the acoustic observations are identical.
class ArticulationTracker {
public:
    struct Observation {
        float pitch=-1,confidence=0;
        // Independent waveform/partial-phase change, not an RMS attack flag.
        float excitation=0;
        int64_t sample=0;
        // Length of the actual pitch observation, at this object's sample rate.
        int observationSamples=0;
    };
    struct Decision {
        bool newNote=false,moving=false;
        int note=-1;
        float pitch=-1,confidence=0;
    };
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void begin(int note,float pitch,int64_t sample) noexcept;
    Decision observe(const Observation&) noexcept;
    bool isMoving() const noexcept {return moving;}
    bool active() const noexcept {return anchor>=0;}
private:
    double rate=12000;
    int anchor=-1,flat=0;
    int previousObservationSamples=0,movementObservationSamples=0;
    float reference=-1,origin=-1,previous=-1,peakSpeed=0;
    int64_t previousSample=0,lastNearReference=0,movementStart=0;
    bool moving=false;
};

// The phase relationships between harmonics survive an ordinary continuous
// pitch bend. A new excitation may change them without an amplitude attack.
// This supplies independent evidence to the trajectory classifier; it never
// decides that an arbitrary phase change is a new musical note by itself.
class PartialPhaseTracker {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    float observe(const float* wave,int samples,float frequency,int64_t sample) noexcept;
private:
    double rate=12000;
    std::array<double,3> previous{},trend{},departure{};
    std::array<double,4> previousPower{},powerDeparture{};
    std::array<bool,3> valid{};
    int64_t previousSample=0,excitationSample=0;
    float previousPitch=-1,heldExcitation=0;
    bool powerValid=false;
};
}
