// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>

namespace flysnapper::pitch {
// Causal dynamics before pitch analysis. No lookahead, delay line, allocation,
// lock or IO is used. The caller owns this object on the audio thread and must
// publish copies of Meter/CalibrationStatus for other threads.
class InputDynamics {
public:
    struct Settings {
        bool enabled=false;
        float inputGainDb=0;
        float gateThresholdDb=-60;
        float gateHysteresisDb=8;
        float gateAttackMs=.25f;
        float gateHoldMs=35;
        float gateReleaseMs=160;
        float expanderRatio=4;
        float compressorThresholdDb=-24;
        float compressorRatio=3;
        float compressorAttackMs=3;
        float compressorReleaseMs=120;
        float makeupGainDb=0;
        float limiterCeilingDb=-3;
    };
    struct Meter {
        float rawRmsDb=-120;       // Before trim/dynamics; suitable for velocity.
        float inputEnvelopeDb=-120;// After input trim, before expansion.
        float processedDb=-120;
        float gainDb=0;           // Complete applied gain, including trim.
        bool gateOpen=false;      // Latched open, including hysteresis/hold.
        bool admissionOpen=false; // Independently above opening threshold NOW.
        bool limiting=false;
    };
    enum class CalibrationPhase : uint8_t { Idle,Noise,Playing,Complete,Failed,Cancelled };
    enum class CalibrationFailure : uint8_t { None,NoPlaying,InsufficientSeparation,Clipping,InvalidInput };
    struct CalibrationResult {
        Settings settings{};
        float noiseFloorDb=-120;  // 95th percentile 10-ms quiet RMS.
        float noisePeakDb=-120;   // 95th percentile quiet block peaks.
        float softSignalDb=-120;  // 20th percentile active playing RMS.
        float strongSignalDb=-120;// 90th percentile active playing RMS.
        float decayDbPerSecond=0;// Median measured moderate decay; zero if absent.
        float usableSeconds=0;
        float tonalSeconds=0;    // Coarsely periodic active calibration windows.
        float clippedFraction=0;
        CalibrationFailure failure=CalibrationFailure::None;
        bool success=false;
    };
    struct CalibrationStatus {
        CalibrationPhase phase=CalibrationPhase::Idle;
        float progress=0;
        float remainingSeconds=0; // Total20s remaining; quiet countdown is this−17.
        CalibrationResult result{};
    };
    static constexpr float noiseSeconds=3,playingSeconds=17,totalSeconds=20;

    explicit InputDynamics(double sampleRate=48000);
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void configure(const Settings&) noexcept;
    Settings settings() const noexcept {return config;}
    float processSample(float input) noexcept;
    Meter meter() const noexcept;
    bool allowsNewNotes() const noexcept {return !config.enabled||admission;}
    void startCalibration() noexcept;
    void cancelCalibration() noexcept;
    CalibrationStatus calibrationStatus() const noexcept;
    bool isCalibrating() const noexcept;
    CalibrationPhase phase() const noexcept {return calibrationPhase;}
    double sampleRate() const noexcept {return rate;}
    // Stable finite ranges, also used when restoring automation/preset values.
    static Settings sanitise(const Settings&) noexcept;

private:
    static constexpr int histogramBins=145; // One-dB bins: -120 through +24.
    struct Histogram {
        std::array<uint32_t,histogramBins> counts{};
        uint32_t total=0;
        void add(float db) noexcept;
        float quantile(float fraction) const noexcept;
        uint32_t above(float db) const noexcept;
        float quantileAbove(float db,float fraction) const noexcept;
    };
    Settings config{};
    double rate=48000;
    double inputGain=1,makeupGain=1,openLevel=.001,closeLevel=.000398107,ceiling=.7079458,compressorLevel=.0630957;
    double envelopeRelease=0,gateAttack=0,gateRelease=0,compAttack=0,compRelease=0,limiterRelease=0,rmsCoefficient=0;
    double detectorEnvelope=0,compressorEnvelope=0,gateGain=0,compressorGain=1,limiterGain=1;
    double rawPower=0,outputPower=0,lastTotalGain=1;
    int holdRemaining=0,holdSamples=0;
    bool open=false,admission=false,limited=false;
    CalibrationPhase calibrationPhase=CalibrationPhase::Idle;
    CalibrationResult calibrationResult{};
    Histogram noiseRms{},noisePeaks{},playingRms{},playingPeaks{};
    std::array<uint32_t,161> decayRates{};
    uint64_t calibrationSamples=0,noiseSampleCount=0,totalSampleCount=0;
    uint64_t clippedSamples=0,invalidSamples=0;
    int blockSamples=0,blockCount=0;
    double blockPower=0,blockPeak=0;
    double decayEnvelopePower=0,previousDecayDb=-120;
    bool decayReady=false;
    std::array<float,512> calibrationHistory{};
    double calibrationLow1=0,calibrationLow2=0,calibrationPhaseStep=0,calibrationLowpass=0;
    int calibrationWrite=0,calibrationFilled=0,calibrationSinceCheck=0;
    uint32_t tonalWindows=0;
    void observeCalibrationPeriodicity(float input) noexcept;
    void finishCalibrationBlock(bool noise) noexcept;
    void updateCoefficients() noexcept;
    void observeCalibration(float trimmed,bool clipped,bool invalid) noexcept;
    void finishCalibration() noexcept;
};
}
