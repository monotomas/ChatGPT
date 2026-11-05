// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <limits>

juce::String FlysnapperProcessor::mappingParameterId(int type) {
    return "map_"+juce::String(flysnapper::chordTypes()[size_t(type)].id);
}
juce::AudioProcessorValueTreeState::ParameterLayout FlysnapperProcessor::parameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"correction",1},"Correction",true));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},"Mode",juce::StringArray{"Chord Notes","Chord Scale","Fixed Scale"},2));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"root",1},"Root",juce::StringArray{"C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"},0));
    juce::StringArray scales; for(const auto& s:flysnapper::scaleDefinitions())scales.add(s.name);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scale",1},"Scale",scales,0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"routingTest",1},"Routing Test",false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"pitchEnabled",1},"Pitch Enabled",true));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"pitchMode",1},"Pitch Mode",juce::StringArray{"Mono","Poly"},0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"monoBend",1},"Mono Bend",false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"mpeOutput",1},"MPE Output",false));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"bendRange",1},"Bend Range",juce::StringArray{"2 semitones","12 semitones"},0));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"inputGate",1},"Input Threshold",juce::NormalisableRange<float>(-80.0f,-20.0f,1.0f),-55.0f));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"autoInput",1},"Automatic Input",true));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sensitivity",1},"Sensitivity",juce::NormalisableRange<float>(.35f,.95f,.01f),.65f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"releaseMs",1},"Release",juce::NormalisableRange<float>(10.0f,150.0f,1.0f),35.0f));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"dynamicsEnabled",1},"Input Dynamics",true));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"inputGainDb",1},"Input Gain",juce::NormalisableRange<float>(-24.0f,24.0f,.1f),0.0f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gateThresholdDb",1},"Gate Open Threshold",juce::NormalisableRange<float>(-96.0f,-12.0f,.1f),-60.0f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gateHysteresisDb",1},"Gate Hysteresis",juce::NormalisableRange<float>(0.0f,24.0f,.1f),8.0f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gateAttackMs",1},"Gate Attack",juce::NormalisableRange<float>(.05f,10.0f,.05f),.25f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gateHoldMs",1},"Gate Hold",juce::NormalisableRange<float>(0.0f,250.0f,1.0f),35.0f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gateReleaseMs",1},"Gate Release",juce::NormalisableRange<float>(10.0f,2000.0f,1.0f),160.0f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"compressorThresholdDb",1},"Compressor Threshold",juce::NormalisableRange<float>(-60.0f,0.0f,.1f),-24.0f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"compressorRatio",1},"Compressor Ratio",juce::NormalisableRange<float>(1.0f,12.0f,.1f),3.0f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"limiterCeilingDb",1},"Limiter Ceiling",juce::NormalisableRange<float>(-12.0f,0.0f,.1f),-3.0f));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"help",1},"Context Help",true));
    for(int i=0;i<50;++i) {
        const auto& c=flysnapper::chordTypes()[size_t(i)];
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{mappingParameterId(i),1},"Map "+juce::String(c.id),scales,c.defaultScale));
    }
    return p;
}
FlysnapperProcessor::FlysnapperProcessor()
    : AudioProcessor(BusesProperties().withInput("Sidechain",juce::AudioChannelSet::stereo(),false)
        .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"FLYSNAPPER_PARAMETERS",parameterLayout()) {
    correction=parameters.getRawParameterValue("correction");mode=parameters.getRawParameterValue("mode");
    root=parameters.getRawParameterValue("root");scale=parameters.getRawParameterValue("scale");
    routingTest=parameters.getRawParameterValue("routingTest");
    pitchEnabled=parameters.getRawParameterValue("pitchEnabled");pitchMode=parameters.getRawParameterValue("pitchMode");
    inputGate=parameters.getRawParameterValue("inputGate");sensitivity=parameters.getRawParameterValue("sensitivity");
    releaseMs=parameters.getRawParameterValue("releaseMs");
    monoBend=parameters.getRawParameterValue("monoBend");bendRange=parameters.getRawParameterValue("bendRange");
    mpeOutput=parameters.getRawParameterValue("mpeOutput");autoInput=parameters.getRawParameterValue("autoInput");
    dynamicsEnabled=parameters.getRawParameterValue("dynamicsEnabled");
    for(size_t i=0;i<dynamicsParameterCount;++i)dynamicsValues[i]=parameters.getRawParameterValue(dynamicsParameterIds[i]);
    for(int i=0;i<50;++i)mapValues[size_t(i)]=parameters.getRawParameterValue(mappingParameterId(i));
    result.ensureSize(131072);display.store(flysnapper::packSnapshot(engine.snapshot()));
    setLatencySamples(0);
    // Poll an atomic result on the message thread. Audio processing neither
    // posts GUI messages nor calls host parameter/gesture callbacks.
    startTimerHz(20);
}
FlysnapperProcessor::~FlysnapperProcessor(){stopTimer();}
flysnapper::pitch::InputDynamics::Settings FlysnapperProcessor::dynamicsSettings() const noexcept {
    flysnapper::pitch::InputDynamics::Settings s;
    s.enabled=dynamicsEnabled->load(std::memory_order_relaxed)>=.5f;
    s.inputGainDb=dynamicsValues[0]->load(std::memory_order_relaxed);
    s.gateThresholdDb=dynamicsValues[1]->load(std::memory_order_relaxed);
    s.gateHysteresisDb=dynamicsValues[2]->load(std::memory_order_relaxed);
    s.gateAttackMs=dynamicsValues[3]->load(std::memory_order_relaxed);
    s.gateHoldMs=dynamicsValues[4]->load(std::memory_order_relaxed);
    s.gateReleaseMs=dynamicsValues[5]->load(std::memory_order_relaxed);
    s.compressorThresholdDb=dynamicsValues[6]->load(std::memory_order_relaxed);
    s.compressorRatio=dynamicsValues[7]->load(std::memory_order_relaxed);
    s.limiterCeilingDb=dynamicsValues[8]->load(std::memory_order_relaxed);
    return s;
}
void FlysnapperProcessor::requestInputCalibration() noexcept {
    calibrationEpoch.fetch_add(1,std::memory_order_acq_rel);
    proposalPending.store(false,std::memory_order_release);
    calibrationApplied.store(false,std::memory_order_relaxed);
    calibrationProgress.store(0,std::memory_order_relaxed);
    calibrationRemaining.store(flysnapper::pitch::InputDynamics::totalSeconds,std::memory_order_relaxed);
    calibrationFailure.store(0,std::memory_order_relaxed);
    calibrationClipped.store(false,std::memory_order_relaxed);
    calibrationPhase.store(int(InputCalibrationSnapshot::Phase::Idle),std::memory_order_release);
    calibrationPending.store(true,std::memory_order_release);
    calibrationCommand.store(1,std::memory_order_release);
}
void FlysnapperProcessor::cancelInputCalibration() noexcept {
    calibrationEpoch.fetch_add(1,std::memory_order_acq_rel);
    proposalPending.store(false,std::memory_order_release);
    calibrationPending.store(false,std::memory_order_release);
    calibrationApplied.store(false,std::memory_order_relaxed);
    calibrationPhase.store(int(InputCalibrationSnapshot::Phase::Cancelled),std::memory_order_release);
    calibrationCommand.store(2,std::memory_order_release);
}
FlysnapperProcessor::InputCalibrationSnapshot FlysnapperProcessor::getInputCalibrationSnapshot() const noexcept {
    InputCalibrationSnapshot s;
    s.phase=static_cast<InputCalibrationSnapshot::Phase>(calibrationPhase.load(std::memory_order_acquire));
    s.progress=calibrationProgress.load(std::memory_order_relaxed);
    s.secondsRemaining=calibrationRemaining.load(std::memory_order_relaxed);
    s.noiseFloorDb=calibrationNoise.load(std::memory_order_relaxed);
    s.softLevelDb=calibrationSoft.load(std::memory_order_relaxed);
    s.strongLevelDb=calibrationStrong.load(std::memory_order_relaxed);
    s.clipped=calibrationClipped.load(std::memory_order_relaxed);
    s.failureReason=calibrationFailure.load(std::memory_order_relaxed);
    s.applied=calibrationApplied.load(std::memory_order_acquire);
    s.pending=calibrationPending.load(std::memory_order_acquire);
    return s;
}
void FlysnapperProcessor::failCalibration(int reason) noexcept {
    dynamics.cancelCalibration();proposalPending.store(false,std::memory_order_release);
    calibrationPending.store(false,std::memory_order_release);
    calibrationApplied.store(false,std::memory_order_relaxed);
    calibrationFailure.store(reason,std::memory_order_relaxed);
    calibrationPhase.store(int(InputCalibrationSnapshot::Phase::Failed),std::memory_order_release);
}
void FlysnapperProcessor::publishCalibration() noexcept {
    if(runningCalibrationEpoch!=calibrationEpoch.load(std::memory_order_acquire))return;
    const auto s=dynamics.calibrationStatus();
    calibrationProgress.store(s.progress,std::memory_order_relaxed);
    calibrationRemaining.store(s.remainingSeconds,std::memory_order_relaxed);
    calibrationNoise.store(s.result.noiseFloorDb,std::memory_order_relaxed);
    calibrationSoft.store(s.result.softSignalDb,std::memory_order_relaxed);
    calibrationStrong.store(s.result.strongSignalDb,std::memory_order_relaxed);
    calibrationClipped.store(s.result.clippedFraction>0,std::memory_order_relaxed);
    calibrationFailure.store(int(s.result.failure),std::memory_order_relaxed);
    calibrationPhase.store(int(s.phase),std::memory_order_release);
    if(s.phase==flysnapper::pitch::InputDynamics::CalibrationPhase::Complete&&s.result.success
        &&runningCalibrationEpoch==calibrationEpoch.load(std::memory_order_acquire)
        &&!calibrationProposalPublished) {
        const auto& p=s.result.settings;
        const std::array<float,dynamicsParameterCount> values{{p.inputGainDb,p.gateThresholdDb,p.gateHysteresisDb,p.gateAttackMs,
            p.gateHoldMs,p.gateReleaseMs,p.compressorThresholdDb,p.compressorRatio,p.limiterCeilingDb}};
        for(size_t i=0;i<values.size();++i)calibrationProposal[i].store(values[i],std::memory_order_relaxed);
        proposalEpoch.store(runningCalibrationEpoch,std::memory_order_relaxed);
        calibrationProposalPublished=true;
        calibrationPending.store(true,std::memory_order_release);
        proposalPending.store(true,std::memory_order_release);
    }
}
void FlysnapperProcessor::timerCallback() {
    const juce::ScopedLock applyLock(calibrationApplyLock);
    if(!proposalPending.exchange(false,std::memory_order_acq_rel))return;
    const auto epoch=proposalEpoch.load(std::memory_order_acquire);
    if(epoch!=calibrationEpoch.load(std::memory_order_acquire))return;
    bool changed=dynamicsEnabled->load()!=calibrationStartEnabled.load(std::memory_order_relaxed);
    for(size_t i=0;i<dynamicsParameterCount;++i)
        changed=changed||dynamicsValues[i]->load()!=calibrationStartValues[i].load(std::memory_order_relaxed);
    if(changed) {
        calibrationEpoch.fetch_add(1,std::memory_order_acq_rel);
        calibrationPending.store(false,std::memory_order_release);
        calibrationFailure.store(6,std::memory_order_relaxed);
        calibrationPhase.store(int(InputCalibrationSnapshot::Phase::Failed),std::memory_order_release);return;
    }
    // One completed proposal updates the real host parameters/knobs. An
    // unsuccessful or cancelled capture never touches the current settings.
    for(size_t i=0;i<dynamicsParameterCount;++i) {
        auto* p=parameters.getParameter(dynamicsParameterIds[i]);
        const auto normal=p->convertTo0to1(calibrationProposal[i].load(std::memory_order_relaxed));
        if(p->getValue()!=normal){p->beginChangeGesture();p->setValueNotifyingHost(normal);p->endChangeGesture();}
    }
    auto* enable=parameters.getParameter("dynamicsEnabled");
    if(enable->getValue()<.5f){enable->beginChangeGesture();enable->setValueNotifyingHost(1);enable->endChangeGesture();}
    calibrationApplied.store(true,std::memory_order_release);
    calibrationPending.store(false,std::memory_order_release);
}
bool FlysnapperProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.outputBuses.size()==1 && l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()
        && l.inputBuses.size()==1 && (l.getMainInputChannelSet().isDisabled()
            || l.getMainInputChannelSet()==juce::AudioChannelSet::mono()
            || l.getMainInputChannelSet()==juce::AudioChannelSet::stereo());
}
void FlysnapperProcessor::prepareToPlay(double rate,int) {
    sampleRate=std::isfinite(rate)&&rate>=8000&&rate<=384000?rate:48000;result.ensureSize(131072);
    detector.prepare(sampleRate);
    dynamics.prepare(sampleRate);dynamics.configure(dynamicsSettings());
    cancelInputCalibration();calibrationCommand.store(0);calibrationMuted=false;
    calibrationPhase.store(int(InputCalibrationSnapshot::Phase::Idle));
    needsReset.store(true);testRemaining=-1;testWasHigh=false;wasPlaying=false;havePosition=false;
    wasPitchEnabled=false;previousPitchMode=-1;inputLevel.store(-120.0f);detectedMask.store(0);
}
void FlysnapperProcessor::releaseResources() {needsReset.store(true);testRemaining=-1;cancelInputCalibration();}
void FlysnapperProcessor::processBlock(juce::AudioBuffer<float>& a,juce::MidiBuffer& m){process(a,m,false);}
void FlysnapperProcessor::processBlockBypassed(juce::AudioBuffer<float>& a,juce::MidiBuffer& m){process(a,m,true);}
void FlysnapperProcessor::process(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi,bool bypassed) {
    juce::ScopedNoDenormals noDenormals;
    const int samples=audio.getNumSamples();
    result.clear();output.clear();
    mpeRouter.beginBlock(mpePackets);
    unsigned noteCount=0;
    bool emittingMpe=previousMpeOutput;
    auto drain=[&] {
        if(emittingMpe){mpeRouter.consumeCore(output,mpePackets);return;}
        for(size_t k=0;k<output.size;++k) {
            const auto& e=output.events[k];
            const int command=e.type==flysnapper::EventType::NoteOn?0x90:e.type==flysnapper::EventType::NoteOff?0x80:0xb0;
            const juce::uint8 raw[]{juce::uint8(command|(e.channel-1)),juce::uint8(e.note),juce::uint8(e.value)};
            result.addEvent(raw,3,e.offset);if(e.type==flysnapper::EventType::NoteOn)++noteCount;
        }
        output.size=0;
    };
    const auto flushMpe=[&] {
        for(size_t i=0;i<mpePackets.size;++i) {
            const auto& e=mpePackets.events[i];result.addEvent(e.bytes.data(),e.length,e.offset);
            if((e.bytes[0]&0xf0)==0x90&&e.bytes[2]>0)++noteCount;
        }
        mpePackets.clear();
    };
    bool resetNow=needsReset.exchange(false);
    bool resetBendNow=false;
    if(auto* head=getPlayHead())if(auto pos=head->getPosition()) {
        const bool playing=pos->getIsPlaying();
        if(wasPlaying&&!playing)resetNow=true;
        if(auto time=pos->getTimeInSamples()) {
            if(playing&&wasPlaying&&havePosition&&*time!=expectedPosition)resetNow=true;
            expectedPosition=*time+samples;havePosition=true;
        } else havePosition=false;
        wasPlaying=playing;
    }
    const bool currentMpe=mpeOutput->load()>=.5f;
    const bool outputModeChanged=currentMpe!=previousMpeOutput;
    if(resetNow) {
        engine.panic(0,output);engine.clearChord();testRemaining=-1;
        detectedEvents.clear();detector.reset(detectedEvents,0);detectedEvents.clear();
        resetBendNow=true;
    }
    if(outputModeChanged) {
        // Member allocation changes the core channels as well as the wire
        // format. Release in the previous format before accepting new notes;
        // the held chord input remains valid across this output-only change.
        for(auto source:{flysnapper::Source::External,flysnapper::Source::Generated,flysnapper::Source::Diagnostic})
            engine.stopSource(source,0,output);
        if(!previousMpeOutput)for(int ch=1;ch<=16;++ch) {
            if(legacyHolds[size_t(ch-1)]&1)engine.controller(flysnapper::Source::External,ch,66,0,0,output);
            if(legacyHolds[size_t(ch-1)]&2)engine.controller(flysnapper::Source::External,ch,69,0,0,output);
        }
        detectedEvents.clear();detector.reset(detectedEvents,0);detectedEvents.clear();
        testRemaining=-1;resetBendNow=true;
    }
    if(resetNow||outputModeChanged) {
        drain();
        if(previousMpeOutput){mpeRouter.clear(mpePackets);flushMpe();}
        else if(bendSession) {
            const juce::uint8 centre[]{0xe1,0,64};result.addEvent(centre,3,0);
        }
        bendSession=false;bendConfiguredRange=0;legacyHolds={};
    }
    emittingMpe=currentMpe;previousMpeOutput=currentMpe;
    mpeRouter.beginBlock(mpePackets,int(flysnapper::midi::Packets::capacity)-result.getNumEvents());
    flysnapper::Settings settings;
    settings.correctionEnabled=correction->load()>=.5f&&!bypassed;
    settings.mode=static_cast<flysnapper::Mode>(int(mode->load()));settings.root=int(root->load());settings.scale=int(scale->load());
    for(int i=0;i<50;++i)settings.chordScales[size_t(i)]=int(mapValues[size_t(i)]->load());
    engine.setSettings(settings);
    bool bus=getBus(true,0)->isEnabled();sidechain.store(bus,std::memory_order_relaxed);
    const bool pitchActive=bus&&!bypassed&&pitchEnabled->load()>=.5f;
    // Check manual/host overrides before configure(), which itself cancels
    // calibration when input trim changes.
    if(dynamics.isCalibrating()) {
        bool changed=dynamicsEnabled->load()!=calibrationStartEnabled.load(std::memory_order_relaxed);
        for(size_t i=0;i<dynamicsParameterCount;++i)
            changed=changed||dynamicsValues[i]->load()!=calibrationStartValues[i].load(std::memory_order_relaxed);
        if(changed)failCalibration(6);
    }
    dynamics.configure(dynamicsSettings());
    const int calibrationAction=calibrationCommand.exchange(0,std::memory_order_acq_rel);
    if(calibrationAction==2) {
        dynamics.cancelCalibration();
        calibrationPhase.store(int(InputCalibrationSnapshot::Phase::Cancelled),std::memory_order_release);
    }
    if(calibrationAction==1) {
        runningCalibrationEpoch=calibrationEpoch.load(std::memory_order_acquire);
        calibrationProposalPublished=false;
        calibrationApplied.store(false,std::memory_order_relaxed);
        calibrationPending.store(false,std::memory_order_release);
        if(!bus||bypassed)failCalibration(5);
        else {
            calibrationStartEnabled.store(dynamicsEnabled->load(),std::memory_order_relaxed);
            for(size_t i=0;i<dynamicsParameterCount;++i)
                calibrationStartValues[i].store(dynamicsValues[i]->load(),std::memory_order_relaxed);
            dynamics.startCalibration();publishCalibration();
        }
    }
    if(dynamics.isCalibrating()) {
        bool changed=dynamicsEnabled->load()!=calibrationStartEnabled.load(std::memory_order_relaxed);
        for(size_t i=0;i<dynamicsParameterCount;++i)
            changed=changed||dynamicsValues[i]->load()!=calibrationStartValues[i].load(std::memory_order_relaxed);
        if(!bus||bypassed)failCalibration(5);
        else if(changed)failCalibration(6);
    }
    const bool muteForCalibration=dynamics.isCalibrating()||calibrationPending.load(std::memory_order_acquire);
    if(muteForCalibration!=calibrationMuted) {
        engine.stopSource(flysnapper::Source::Generated,0,output);
        detectedEvents.clear();detector.reset(detectedEvents,0);detectedEvents.clear();
        resetBendNow=true;calibrationMuted=muteForCalibration;
    }
    const int currentPitchMode=int(pitchMode->load());
    const bool currentMonoBend=currentPitchMode==0&&(currentMpe||monoBend->load()>=.5f);
    const int currentBendRange=currentMpe?flysnapper::midi::MpeOutputRouter::memberRange:(bendRange->load()>=.5f?12:2);
    if((wasPitchEnabled&&!pitchActive)||(previousPitchMode>=0&&previousPitchMode!=currentPitchMode)
        ||previousMonoBend!=currentMonoBend||(currentMonoBend&&previousBendRange!=currentBendRange)) {
        engine.stopSource(flysnapper::Source::Generated,0,output);
        detectedEvents.clear();detector.reset(detectedEvents,0);detectedEvents.clear();
        resetBendNow=true;
    }
    if(bypassed) {engine.stopSource(flysnapper::Source::Diagnostic,0,output);testRemaining=-1;}
    wasSidechain=bus;wasPitchEnabled=pitchActive;previousPitchMode=currentPitchMode;
    previousMonoBend=currentMonoBend;previousBendRange=currentBendRange;
    flysnapper::pitch::Settings pitchSettings;
    pitchSettings.enabled=pitchActive&&!calibrationMuted;pitchSettings.polyphonic=currentPitchMode==1;
    pitchSettings.thresholdDb=inputGate->load();pitchSettings.sensitivity=sensitivity->load();
    pitchSettings.autoInput=autoInput->load()>=.5f;
    pitchSettings.releaseMs=releaseMs->load();
    pitchSettings.monoBend=currentMonoBend;
    // Polyphony is deliberately quantised to semitone notes. MPE may still
    // carry separate notes, but it no longer enables polyphonic bending.
    pitchSettings.polyBend=false;
    pitchSettings.bendRangeSemitones=float(currentBendRange);detector.configure(pitchSettings);
    // Bus activation is the host's only reliable connectivity signal. Digital
    // silence is processed normally and releases notes; it is not No Input.
    size_t count=0;bool inputOverflow=false;
    auto append=[&](int offset,const juce::uint8* data,int length,flysnapper::Source source)->Input* {
        if(count==inputCapacity){inputOverflow=true;return nullptr;}
        auto& item=inputs[count];item.offset=std::clamp(offset,0,std::max(0,samples-1));
        item.sequence=unsigned(count);item.source=source;item.length=length;
        item.voiceId=-1;item.pitchChange=false;item.pitch=0;
        std::copy_n(data,length,item.data.begin());++count;return &item;
    };
    for(const auto meta:midi) {
        // VST3 is not a raw SysEx cable. Explicitly count unsupported payloads.
        if(meta.numBytes<1||meta.numBytes>3){dropped.fetch_add(1,std::memory_order_relaxed);continue;}
        append(meta.samplePosition,meta.data,meta.numBytes,flysnapper::Source::External);
    }
    const bool testHigh=routingTest->load()>=.5f;
    const bool buttonRequest=testRequested.exchange(false);
    const bool testStart=(testHigh&&!testWasHigh)||buttonRequest;
    if(!testHigh&&testWasHigh){engine.stopSource(flysnapper::Source::Diagnostic,0,output);testRemaining=-1;}
    testWasHigh=testHigh;
    if(samples>0&&!bypassed&&testStart) {
        engine.stopSource(flysnapper::Source::Diagnostic,0,output);
        const juce::uint8 on[]{0x90,60,100};append(0,on,3,flysnapper::Source::Diagnostic);
        testRemaining=std::max<int64_t>(1,std::llround(sampleRate*.1));
    }
    if(samples>0&&testRemaining>=0) {
        if(testRemaining<samples) {
            const juce::uint8 off[]{0x80,60,0};append(int(testRemaining),off,3,flysnapper::Source::Diagnostic);testRemaining=-1;
        } else testRemaining-=samples;
    }
    if(bus&&!bypassed&&samples>0&&(pitchActive||dynamics.isCalibrating()||calibrationMuted)) {
        const auto input=getBusBuffer(audio,true,0);
        const int channels=input.getNumChannels();
        auto clean=[](float value) noexcept {
            return std::isfinite(value)?std::clamp(value,-16.0f,16.0f):0.0f;
        };
        double firstEnergy=0,secondEnergy=0,sumEnergy=0;
        if(channels>1)for(int i=0;i<samples;++i) {
            const double a=clean(input.getSample(0,i)),b=clean(input.getSample(1,i));
            firstEnergy+=a*a;secondEnergy+=b*b;sumEnergy+=(a+b)*(a+b);
        }
        // A nearly phase-inverted pair necessarily has meaningful energy in
        // both channels. Keep a fixed left-channel polarity reference here;
        // choosing whichever side is louder can flip the waveform every block
        // when their gains cross, manufacturing a different apparent pitch.
        const bool antiPhase=channels>1&&sumEnergy<.25*std::max(firstEnergy,secondEnergy);
        double rawEnergy=0;
        // Process bounded chunks and retain each sample's gate decision,
        // including hosts that provide larger blocks.
        for(int start=0;start<samples;start+=64) {
            const int n=std::min(64,samples-start);
            std::array<bool,64> admission{};
            const bool wasCalibrating=dynamics.isCalibrating();
            for(int i=0;i<n;++i) {
                const float left=channels?input.getSample(0,start+i):0.0f;
                bool invalid=channels>0&&!std::isfinite(left);
                float value=clean(left);
                // Sanitize each channel before mixing: NaN/Inf in one side
                // must not erase a valid signal in the other side.
                if(channels>1&&!antiPhase) {
                    const float right=input.getSample(1,start+i);
                    invalid=invalid&&!std::isfinite(right);
                    value=.5f*value+.5f*clean(right);
                }
                rawEnergy+=double(value)*value;
                audioScratch[size_t(i)]=dynamics.processSample(invalid?std::numeric_limits<float>::quiet_NaN():value);
                admission[size_t(i)]=dynamics.allowsNewNotes();
            }
            if(wasCalibrating)publishCalibration();
            // Keep every admission transition at its actual sample. Using
            // any-open for a whole host chunk would authorise earlier pitch
            // decisions from a gate that only opens later in that chunk.
            for(int first=0;first<n;) {
                int end=first+1;while(end<n&&admission[size_t(end)]==admission[size_t(first)])++end;
                pitchSettings.allowNewNotes=admission[size_t(first)];
                detector.configure(pitchSettings);
                detectedEvents.clear();detector.process(audioScratch.data()+first,end-first,detectedEvents);
                for(size_t i=0;i<detectedEvents.size;++i) {
                    const auto& event=detectedEvents.events[i];
                    if(currentMpe) {
                        if(event.voiceId<0||event.voiceId>=flysnapper::midi::MpeOutputRouter::generatedVoices
                            ||(event.pitchChange&&!std::isfinite(event.pitch))){inputOverflow=true;break;}
                        const juce::uint8 raw[]{juce::uint8(event.pitchChange?0xe0:(event.on?0x90:0x80)),juce::uint8(event.note),juce::uint8(event.velocity)};
                        if(auto* item=append(start+first+event.offset,raw,3,flysnapper::Source::Generated)) {
                            item->voiceId=event.voiceId;item->pitchChange=event.pitchChange;item->pitch=event.pitch;
                        }
                    } else if(event.pitchChange) {
                        if(!currentMonoBend||!std::isfinite(event.pitch))continue;
                        // The detector's acoustic anchor is corrected only at
                        // Note On. Expression remains relative to that anchor,
                        // preserving a bend around the corrected output note.
                        const auto delta=double(event.pitch)-event.note;
                        const int wheel=std::clamp(int(std::lround(8192.+8192.*delta/currentBendRange)),0,16383);
                        const juce::uint8 raw[]{0xe1,juce::uint8(wheel&127),juce::uint8((wheel>>7)&127)};
                        append(start+first+event.offset,raw,3,flysnapper::Source::Generated);
                    } else {
                        const int channelOffset=currentMonoBend?1:0;
                        const juce::uint8 raw[]{juce::uint8((event.on?0x90:0x80)|channelOffset),juce::uint8(event.note),juce::uint8(event.velocity)};
                        append(start+first+event.offset,raw,3,flysnapper::Source::Generated);
                    }
                }
                if(detectedEvents.overflow){inputOverflow=true;break;}
                first=end;
            }
            if(inputOverflow)break;
        }
        const auto status=detector.status();
        inputLevel.store(float(20*std::log10(std::max(1e-6,std::sqrt(rawEnergy/std::max(1,samples))))),std::memory_order_relaxed);
        detectedMask.store(status.activePitchClasses,std::memory_order_relaxed);
        const auto meter=dynamics.meter();
        dynamicsLevel.store(meter.processedDb,std::memory_order_relaxed);
        dynamicsGain.store(meter.gainDb,std::memory_order_relaxed);
        dynamicsGateOpen.store(meter.gateOpen,std::memory_order_relaxed);
    } else {inputLevel.store(-120.0f,std::memory_order_relaxed);detectedMask.store(0,std::memory_order_relaxed);}
    if(inputOverflow) {
        engine.panic(0,output);engine.clearChord();testRemaining=-1;dropped.fetch_add(1);count=0;
        detectedEvents.clear();detector.reset(detectedEvents,0);detectedMask.store(0);
        resetBendNow=true;
    }
    std::sort(inputs.begin(),inputs.begin()+std::ptrdiff_t(count),[](const auto& a,const auto& b){
        return a.offset<b.offset||(a.offset==b.offset&&a.sequence<b.sequence);
    });
    const auto wheel=[&](int value,int offset) {
        const juce::uint8 bytes[]{0xe1,juce::uint8(value&127),juce::uint8((value>>7)&127)};
        result.addEvent(bytes,3,offset);
    };
    const auto centreBend=[&](int offset) {
        if(bendSession)wheel(8192,offset);
        bendSession=false;
        // A receiver may have been inserted or replaced between voices.
        // Re-establish the advertised range before every new bent note.
        bendConfiguredRange=0;
    };
    const auto prepareBend=[&](int offset) {
        if(bendConfiguredRange==currentBendRange)return;
        // Standard MIDI RPN 0: pitch-bend sensitivity. A receiver that ignores
        // RPN must be set to the same 2/12-semitone range manually. This is
        // ordinary channel expression, not MPE or a per-note polyphonic bend.
        for(const auto pair:std::array<std::array<int,2>,6>{{{{101,0}},{{100,0}},{{6,currentBendRange}},{{38,0}},{{101,127}},{{100,127}}}}) {
            const juce::uint8 bytes[]{0xb1,juce::uint8(pair[0]),juce::uint8(pair[1])};
            result.addEvent(bytes,3,offset);
        }
        bendConfiguredRange=currentBendRange;
    };
    drain();
    // All generated Note Offs queued by panic/configuration changes precede
    // the centre message; external channel-1 notes and wheel are untouched.
    if(resetBendNow) {
        if(currentMpe) {
            if(inputOverflow)mpeRouter.clear(mpePackets);
            else mpeRouter.clearGenerated(mpePackets);
        }
        else centreBend(0);
        bendConfiguredRange=0;
    }
    for(size_t i=0;i<count;) {
        size_t end=i+1;while(end<count&&inputs[end].offset==inputs[i].offset)++end;
        int ons=0;
        for(size_t j=i;j<end;++j) {
            const auto& e=inputs[j];const int status=e.data[0]&0xf0,ch=(e.data[0]&15)+1;
            const bool control=e.source==flysnapper::Source::External&&ch==16;
            if(control) {
                if(status==0x90&&e.length==3&&e.data[2]>0)engine.chordNoteOn(e.data[1],e.data[2]);
                else if((status==0x80||status==0x90)&&e.length==3)engine.chordNoteOff(e.data[1]);
                else if(status==0xb0&&e.length==3&&(e.data[1]==120||e.data[1]==123))engine.clearChord();
            } else if(status==0x90&&e.length==3&&e.data[2]>0)++ons;
        }
        engine.refreshChord();engine.beginTimestamp(ons);
        for(size_t j=i;j<end;++j) {
            const auto& e=inputs[j];const int status=e.data[0]&0xf0,ch=(e.data[0]&15)+1;
            if(e.source==flysnapper::Source::External&&ch==16)continue;
            if(currentMpe) {
                if(e.source==flysnapper::Source::Generated) {
                    if(e.pitchChange)mpeRouter.generatedPitch(e.voiceId,e.data[1],e.pitch,e.offset,mpePackets);
                    else if(status==0x90&&e.length==3&&e.data[2]>0)mpeRouter.generatedOn(engine,output,e.voiceId,e.data[1],e.data[2],e.offset,mpePackets);
                    else if((status==0x80||status==0x90)&&e.length==3)mpeRouter.generatedOff(engine,output,e.voiceId,e.data[1],e.data[2],e.offset,mpePackets);
                } else mpeRouter.external(engine,output,e.source,ch,e.data.data(),e.length,e.offset,mpePackets);
                continue;
            }
            mpeRouter.observeExternal(e.source,ch,e.data.data(),e.length);
            if(e.source!=flysnapper::Source::Generated&&status==0xb0&&e.length==3) {
                auto& holds=legacyHolds[size_t(ch-1)];
                if(e.data[1]==121)holds=0;
                else if(e.data[1]==66||e.data[1]==69) {
                    const auto bit=uint8_t(e.data[1]==66?1:2);
                    holds=e.data[2]>=64?uint8_t(holds|bit):uint8_t(holds&~bit);
                }
            }
            const bool generatedBend=e.source==flysnapper::Source::Generated&&currentMonoBend;
            if(generatedBend&&engine.overloaded())continue;
            if(generatedBend&&(status==0xe0||(status==0x90&&e.length==3&&e.data[2]>0))) {
                prepareBend(e.offset);
                // Pitch-change events precede their Note On. A defensive
                // centre also covers a detector Note On without that event.
                if(status!=0xe0&&!bendSession)wheel(8192,e.offset);
                bendSession=true;
            }
            if(status==0x90&&e.length==3&&e.data[2]>0)engine.noteOn(e.source,ch,e.data[1],e.data[2],e.offset,output);
            else if((status==0x80||status==0x90)&&e.length==3)engine.noteOff(e.source,ch,e.data[1],e.data[2],e.offset,output);
            else if(status==0xb0&&e.length==3)engine.controller(e.source,ch,e.data[1],e.data[2],e.offset,output);
            else result.addEvent(e.data.data(),e.length,e.offset); // Channel pressure, bend, etc. preserve as supplied by adapter.
            drain();
            if(generatedBend&&((status==0x80)||(status==0x90&&e.data[2]==0)))centreBend(e.offset);
            if(e.source==flysnapper::Source::External&&ch==2&&status==0xb0&&e.length==3&&e.data[1]==121)
                bendConfiguredRange=0;
        }
        drain();i=end;
    }
    if(output.overflow)dropped.fetch_add(1);
    if(currentMpe) {
        mpeRouter.endBlock(engine,output,mpePackets,std::max(0,samples-1));
        if(mpePackets.overflow)dropped.fetch_add(1);
        flushMpe();
    } else if(engine.overloaded()){centreBend(std::max(0,samples-1));bendConfiguredRange=0;}
    faulted.store(engine.overloaded()||(currentMpe&&mpeRouter.faulted()),std::memory_order_relaxed);
    emitted.fetch_add(noteCount,std::memory_order_relaxed);
    emittedEvents.fetch_add(unsigned(result.getNumEvents()),std::memory_order_relaxed);
    display.store(flysnapper::packSnapshot(engine.snapshot()),std::memory_order_release);
    // Retain our preallocated output buffer instead of swapping in an
    // arbitrary host buffer. The adapter owns/resizes its delivery buffer.
    audio.clear();midi.clear();midi.addEvents(result,0,-1,0);
}
juce::AudioProcessorEditor* FlysnapperProcessor::createEditor(){return new FlysnapperEditor(*this);}
void FlysnapperProcessor::getStateInformation(juce::MemoryBlock& out) {
    const juce::ScopedLock applyLock(calibrationApplyLock);
    juce::XmlElement state("FLYSNAPPER_STATE");state.setAttribute("schema",1);
    auto values=parameters.copyState();
    for(auto child:values)if(child.getProperty("id").toString()=="routingTest")child.setProperty("value",0,nullptr);
    state.addChildElement(values.createXml().release());
    auto* table=state.createNewChildElement("ChordScales");
    for(int i=0;i<50;++i) {
        auto* item=table->createNewChildElement("Choice");item->setAttribute("chord",flysnapper::chordTypes()[size_t(i)].id);
        const int idx=std::clamp(int(mapValues[size_t(i)]->load()),0,18);item->setAttribute("scale",flysnapper::scaleDefinitions()[size_t(idx)].id);
    }
    const int idx=std::clamp(int(scale->load()),0,18);state.setAttribute("fixedScale",flysnapper::scaleDefinitions()[size_t(idx)].id);
    copyXmlToBinary(state,out);
}
void FlysnapperProcessor::setStateInformation(const void* data,int bytes) {
    auto state=getXmlFromBinary(data,bytes);
    if(!state||!state->hasTagName("FLYSNAPPER_STATE")||state->getIntAttribute("schema")!=1)return;
    auto* xml=state->getChildByName("FLYSNAPPER_PARAMETERS");if(!xml)return;
    auto values=juce::ValueTree::fromXml(*xml);if(!values.isValid())return;
    auto set=[&](const juce::String& id,float value) {
        for(auto child:values)if(child.getProperty("id").toString()==id){child.setProperty("value",value,nullptr);return;}
        juce::ValueTree child("PARAM");child.setProperty("id",id,nullptr);child.setProperty("value",value,nullptr);values.addChild(child,-1,nullptr);
    };
    set("routingTest",0);
    // Old schema-1 presets predate expression. Loading one into an instance
    // already using bend must restore the legacy note-only/channel-1 mode.
    for(const char* id:{"monoBend","bendRange","mpeOutput","autoInput","dynamicsEnabled","help"}) {
        bool present=false;for(auto child:values)if(child.getProperty("id").toString()==id){present=true;break;}
        if(!present)set(id,(juce::String(id)=="autoInput"||juce::String(id)=="help")?1:0);
    }
    const std::array<float,dynamicsParameterCount> dynamicsDefaults{{0,-60,8,.25f,35,160,-24,3,-3}};
    for(size_t i=0;i<dynamicsParameterCount;++i) {
        bool present=false;for(auto child:values)if(child.getProperty("id").toString()==dynamicsParameterIds[i]){present=true;break;}
        if(!present)set(dynamicsParameterIds[i],dynamicsDefaults[i]);
    }
    // Stable string IDs are authoritative. Unknown/new types retain defaults.
    const auto defaults=flysnapper::factoryDefaults();for(int i=0;i<50;++i)set(mappingParameterId(i),defaults[size_t(i)]);
    if(auto* table=state->getChildByName("ChordScales"))for(auto* item:table->getChildIterator()) {
        int chord=flysnapper::chordIndexFromId(item->getStringAttribute("chord").toStdString());
        int s=flysnapper::scaleIndexFromId(item->getStringAttribute("scale").toStdString());
        if(chord>=0&&s>=0)set(mappingParameterId(chord),s);
    }
    const int s=flysnapper::scaleIndexFromId(state->getStringAttribute("fixedScale").toStdString());if(s>=0)set("scale",s);
    const juce::ScopedLock applyLock(calibrationApplyLock);
    cancelInputCalibration();parameters.replaceState(values);testRequested.store(false);needsReset.store(true);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FlysnapperProcessor;}
