// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/MidiEngine.h"
#include "Pitch/PitchDetector.h"
#include "Pitch/MpeOutputRouter.h"
#include "Pitch/InputDynamics.h"
#include <atomic>

class FlysnapperProcessor final : public juce::AudioProcessor, public juce::VST3ClientExtensions, private juce::Timer {
public:
    FlysnapperProcessor();
    ~FlysnapperProcessor() override;
    const juce::String getName() const override { return "Flysnapper"; }
    void prepareToPlay(double,int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override {return true;}
    bool acceptsMidi() const override {return true;}
    bool producesMidi() const override {return true;}
    bool isMidiEffect() const override {return false;}
    bool supportsMPE() const override {return false;}
    bool getPluginHasMainInput() const override {return false;}
    double getTailLengthSeconds() const override {return 0;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Default";}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    void reset() override { needsReset.store(true); }
    static juce::String mappingParameterId(int);
    flysnapper::Snapshot getSnapshot() const {return flysnapper::unpackSnapshot(display.load(std::memory_order_acquire));}
    bool sidechainActive() const {return sidechain.load(std::memory_order_relaxed);}
    float getInputLevelDb() const {return inputLevel.load(std::memory_order_relaxed);}
    uint16_t getDetectedMask() const {return detectedMask.load(std::memory_order_relaxed);}
    unsigned outputNotes() const {return emitted.load(std::memory_order_relaxed);}
    unsigned outputEvents() const {return emittedEvents.load(std::memory_order_relaxed);}
    bool capacityFault() const {return faulted.load(std::memory_order_relaxed);}
    unsigned droppedEvents() const {return dropped.load(std::memory_order_relaxed);}
    void requestTestNote() noexcept {testRequested.store(true);}
    void requestPanic() noexcept {needsReset.store(true);}
    struct InputCalibrationSnapshot {
        enum class Phase {Idle,Noise,Playing,Complete,Failed,Cancelled};
        Phase phase=Phase::Idle;
        float progress=0,secondsRemaining=0,noiseFloorDb=-120,softLevelDb=-120,strongLevelDb=-120;
        bool clipped=false;
        int failureReason=0;
        bool applied=false,pending=false;
    };
    void requestInputCalibration() noexcept;
    void cancelInputCalibration() noexcept;
    InputCalibrationSnapshot getInputCalibrationSnapshot() const noexcept;
    float getDynamicsLevelDb() const noexcept {return dynamicsLevel.load(std::memory_order_relaxed);}
    float getDynamicsGainDb() const noexcept {return dynamicsGain.load(std::memory_order_relaxed);}
    bool isDynamicsGateOpen() const noexcept {return dynamicsGateOpen.load(std::memory_order_relaxed);}
    juce::AudioProcessorValueTreeState parameters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout parameterLayout();
    void process(juce::AudioBuffer<float>&,juce::MidiBuffer&,bool bypassed);
    void timerCallback() override;
    flysnapper::pitch::InputDynamics::Settings dynamicsSettings() const noexcept;
    void publishCalibration() noexcept;
    void failCalibration(int reason) noexcept;
    static constexpr size_t dynamicsParameterCount=9;
    static constexpr std::array<const char*,dynamicsParameterCount> dynamicsParameterIds{{
        "inputGainDb","gateThresholdDb","gateHysteresisDb","gateAttackMs","gateHoldMs",
        "gateReleaseMs","compressorThresholdDb","compressorRatio","limiterCeilingDb"}};
    static constexpr size_t inputCapacity=2048;
    struct Input {int offset=0; unsigned sequence=0; flysnapper::Source source=flysnapper::Source::External;
        std::array<juce::uint8,3> data{}; int length=0;
        int voiceId=-1;bool pitchChange=false;float pitch=0;};
    std::array<Input,inputCapacity> inputs{};
    flysnapper::pitch::PitchDetector detector;
    flysnapper::pitch::InputDynamics dynamics;
    flysnapper::pitch::Output detectedEvents;
    std::array<float,256> audioScratch{};
    flysnapper::Engine engine;
    flysnapper::Output output;
    flysnapper::midi::MpeOutputRouter mpeRouter;
    flysnapper::midi::Packets mpePackets;
    juce::MidiBuffer result;
    std::array<std::atomic<float>*,50> mapValues{};
    std::atomic<float> *correction=nullptr,*mode=nullptr,*root=nullptr,*scale=nullptr,*routingTest=nullptr;
    std::atomic<float> *pitchEnabled=nullptr,*pitchMode=nullptr,*inputGate=nullptr,*sensitivity=nullptr,*releaseMs=nullptr;
    std::atomic<float> *monoBend=nullptr,*bendRange=nullptr,*mpeOutput=nullptr,*autoInput=nullptr;
    std::atomic<float>* dynamicsEnabled=nullptr;
    std::array<std::atomic<float>*,dynamicsParameterCount> dynamicsValues{};
    std::array<std::atomic<float>,dynamicsParameterCount> calibrationStartValues{},calibrationProposal{};
    std::atomic<float> calibrationStartEnabled{0};
    std::atomic<unsigned> calibrationEpoch{0},proposalEpoch{0};
    unsigned runningCalibrationEpoch=0;
    bool calibrationProposalPublished=false;
    std::atomic<int> calibrationCommand{0}; // 1=start/restart, 2=cancel; consumed by audio.
    std::atomic<int> calibrationPhase{0},calibrationFailure{0};
    std::atomic<float> calibrationProgress{0},calibrationRemaining{0},calibrationNoise{-120},calibrationSoft{-120},calibrationStrong{-120};
    std::atomic<bool> calibrationClipped{false},calibrationApplied{false},calibrationPending{false},proposalPending{false};
    bool calibrationMuted=false;
    std::atomic<float> dynamicsLevel{-120},dynamicsGain{0};
    std::atomic<bool> dynamicsGateOpen{false};
    // State IO and parameter application run outside processBlock. Serialise
    // them so a preset cannot interleave with a nine-control learned update.
    juce::CriticalSection calibrationApplyLock;
    std::atomic<float> inputLevel{-120.0f};
    std::atomic<uint16_t> detectedMask{0};
    std::atomic<uint64_t> display{0};
    std::atomic<bool> sidechain{false},needsReset{false},testRequested{false},faulted{false};
    std::atomic<unsigned> emitted{0},dropped{0};
    std::atomic<unsigned> emittedEvents{0};
    int64_t testRemaining=-1, expectedPosition=0;
    double sampleRate=48000;
    bool testWasHigh=false,wasPlaying=false,havePosition=false,wasSidechain=false,wasPitchEnabled=false;
    int previousPitchMode=-1;
    // Mono bend reserves output channel 2. Ordinary playing MIDI remains on
    // its original channels; in particular, channel-1 expression is separate.
    bool previousMonoBend=false,bendSession=false;
    bool previousMpeOutput=false;
    std::array<uint8_t,16> legacyHolds{};
    int previousBendRange=2,bendConfiguredRange=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlysnapperProcessor)
};
