// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/MidiEngine.h"
#include <array>
#include <atomic>

// MIDI processing only. A silent stereo instrument output provides the
// host-facing device role; no audio input, pitch detector or dynamics engine.
class FlysnapperProcessor final : public juce::AudioProcessor {
public:
    FlysnapperProcessor();
    ~FlysnapperProcessor() override = default;
    const juce::String getName() const override {return "Flysnapper MIDI";}
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
    double getTailLengthSeconds() const override {return 0;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Default";}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    void reset() override {needsReset.store(true,std::memory_order_release);}
    static juce::String mappingParameterId(int);
    flysnapper::Snapshot getSnapshot() const {return flysnapper::unpackSnapshot(display.load(std::memory_order_acquire));}
    unsigned outputNotes() const {return emitted.load(std::memory_order_relaxed);}
    unsigned outputEvents() const {return emittedEvents.load(std::memory_order_relaxed);}
    bool capacityFault() const {return faulted.load(std::memory_order_relaxed);}
    unsigned droppedEvents() const {return dropped.load(std::memory_order_relaxed);}
    void requestTestNote() noexcept {testRequested.store(true,std::memory_order_release);}
    void requestPanic() noexcept {needsReset.store(true,std::memory_order_release);}
    juce::AudioProcessorValueTreeState parameters;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout parameterLayout();
    void process(juce::AudioBuffer<float>&,juce::MidiBuffer&,bool bypassed);
    static constexpr size_t inputCapacity=2048,maximumInputBytes=65536;
    static constexpr unsigned maximumHeldInputs=2048;
    struct Input {
        const juce::uint8* data=nullptr;
        int length=0,offset=0;
        unsigned sequence=0;
        flysnapper::Source source=flysnapper::Source::External;
    };
    std::array<Input,inputCapacity> inputs{};
    flysnapper::Engine engine;
    flysnapper::Output output;
    juce::MidiBuffer result;
    std::array<std::atomic<float>*,flysnapper::chordTypeCount> mapValues{};
    std::atomic<float> *correction=nullptr,*mode=nullptr,*root=nullptr,*scale=nullptr;
    std::atomic<float> *routingTest=nullptr,*chordThru=nullptr;
    std::array<uint16_t,128> chordHeld{};
    std::array<juce::uint8,128> chordVelocity{};
    unsigned chordHeldCount=0;
    // Other playing channels pass unchanged. Track only receiver cleanup.
    std::array<std::array<uint16_t,128>,16> passthroughHeld{};
    std::array<uint8_t,16> passthroughHolds{};
    unsigned passthroughHeldCount=0;
    std::atomic<uint64_t> display{0};
    std::atomic<bool> needsReset{false},testRequested{false},faulted{false};
    std::atomic<unsigned> emitted{0},emittedEvents{0},dropped{0};
    int64_t testRemaining=-1,expectedPosition=0;
    double sampleRate=48000;
    bool testWasHigh=false,wasPlaying=false,havePosition=false;
    bool previousChordThru=false,ioFault=false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlysnapperProcessor)
};
