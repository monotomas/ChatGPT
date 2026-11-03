// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/MidiEngine.h"
#include "Pitch/PitchDetector.h"
#include <atomic>

class FlysnapperProcessor final : public juce::AudioProcessor, public juce::VST3ClientExtensions {
public:
    FlysnapperProcessor();
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
    juce::AudioProcessorValueTreeState parameters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout parameterLayout();
    void process(juce::AudioBuffer<float>&,juce::MidiBuffer&,bool bypassed);
    static constexpr size_t inputCapacity=2048;
    struct Input {int offset=0; unsigned sequence=0; flysnapper::Source source=flysnapper::Source::External;
        std::array<juce::uint8,3> data{}; int length=0;};
    std::array<Input,inputCapacity> inputs{};
    flysnapper::pitch::PitchDetector detector;
    flysnapper::pitch::Output detectedEvents;
    std::array<float,256> audioScratch{};
    flysnapper::Engine engine;
    flysnapper::Output output;
    juce::MidiBuffer result;
    std::array<std::atomic<float>*,50> mapValues{};
    std::atomic<float> *correction=nullptr,*mode=nullptr,*root=nullptr,*scale=nullptr,*routingTest=nullptr;
    std::atomic<float> *pitchEnabled=nullptr,*pitchMode=nullptr,*inputGate=nullptr,*sensitivity=nullptr,*releaseMs=nullptr;
    std::atomic<float> *monoBend=nullptr,*bendRange=nullptr;
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
    int previousBendRange=2,bendConfiguredRange=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlysnapperProcessor)
};
