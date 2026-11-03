// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

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
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"inputGate",1},"Input Threshold",juce::NormalisableRange<float>(-80.0f,-20.0f,1.0f),-55.0f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sensitivity",1},"Sensitivity",juce::NormalisableRange<float>(.35f,.95f,.01f),.65f));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"releaseMs",1},"Release",juce::NormalisableRange<float>(10.0f,150.0f,1.0f),35.0f));
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
    for(int i=0;i<50;++i)mapValues[size_t(i)]=parameters.getRawParameterValue(mappingParameterId(i));
    result.ensureSize(131072);display.store(flysnapper::packSnapshot(engine.snapshot()));
    setLatencySamples(0);
}
bool FlysnapperProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.outputBuses.size()==1 && l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()
        && l.inputBuses.size()==1 && (l.getMainInputChannelSet().isDisabled()
            || l.getMainInputChannelSet()==juce::AudioChannelSet::mono()
            || l.getMainInputChannelSet()==juce::AudioChannelSet::stereo());
}
void FlysnapperProcessor::prepareToPlay(double rate,int) {
    sampleRate=rate>0?rate:48000;result.ensureSize(131072);
    detector.prepare(sampleRate);
    needsReset.store(true);testRemaining=-1;testWasHigh=false;wasPlaying=false;havePosition=false;
    wasPitchEnabled=false;previousPitchMode=-1;inputLevel.store(-120.0f);detectedMask.store(0);
}
void FlysnapperProcessor::releaseResources() {needsReset.store(true);testRemaining=-1;}
void FlysnapperProcessor::processBlock(juce::AudioBuffer<float>& a,juce::MidiBuffer& m){process(a,m,false);}
void FlysnapperProcessor::processBlockBypassed(juce::AudioBuffer<float>& a,juce::MidiBuffer& m){process(a,m,true);}
void FlysnapperProcessor::process(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi,bool bypassed) {
    juce::ScopedNoDenormals noDenormals;
    const int samples=audio.getNumSamples();
    result.clear();output.clear();
    bool resetNow=needsReset.exchange(false);
    if(auto* head=getPlayHead())if(auto pos=head->getPosition()) {
        const bool playing=pos->getIsPlaying();
        if(wasPlaying&&!playing)resetNow=true;
        if(auto time=pos->getTimeInSamples()) {
            if(playing&&wasPlaying&&havePosition&&*time!=expectedPosition)resetNow=true;
            expectedPosition=*time+samples;havePosition=true;
        } else havePosition=false;
        wasPlaying=playing;
    }
    if(resetNow) {
        engine.panic(0,output);engine.clearChord();testRemaining=-1;
        detectedEvents.clear();detector.reset(detectedEvents,0);detectedEvents.clear();
    }
    flysnapper::Settings settings;
    settings.correctionEnabled=correction->load()>=.5f&&!bypassed;
    settings.mode=static_cast<flysnapper::Mode>(int(mode->load()));settings.root=int(root->load());settings.scale=int(scale->load());
    for(int i=0;i<50;++i)settings.chordScales[size_t(i)]=int(mapValues[size_t(i)]->load());
    engine.setSettings(settings);
    bool bus=getBus(true,0)->isEnabled();sidechain.store(bus,std::memory_order_relaxed);
    const bool pitchActive=bus&&!bypassed&&pitchEnabled->load()>=.5f;
    const int currentPitchMode=int(pitchMode->load());
    if((wasPitchEnabled&&!pitchActive)||(previousPitchMode>=0&&previousPitchMode!=currentPitchMode)) {
        engine.stopSource(flysnapper::Source::Generated,0,output);
        detectedEvents.clear();detector.reset(detectedEvents,0);detectedEvents.clear();
    }
    if(bypassed) {engine.stopSource(flysnapper::Source::Diagnostic,0,output);testRemaining=-1;}
    wasSidechain=bus;wasPitchEnabled=pitchActive;previousPitchMode=currentPitchMode;
    flysnapper::pitch::Settings pitchSettings;
    pitchSettings.enabled=pitchActive;pitchSettings.polyphonic=currentPitchMode==1;
    pitchSettings.thresholdDb=inputGate->load();pitchSettings.sensitivity=sensitivity->load();
    pitchSettings.releaseMs=releaseMs->load();detector.configure(pitchSettings);
    // Bus activation is the host's only reliable connectivity signal. Digital
    // silence is processed normally and releases notes; it is not No Input.
    size_t count=0;bool inputOverflow=false;
    auto append=[&](int offset,const juce::uint8* data,int length,flysnapper::Source source) {
        if(count==inputCapacity){inputOverflow=true;return;}
        auto& item=inputs[count];item.offset=std::clamp(offset,0,std::max(0,samples-1));
        item.sequence=unsigned(count);item.source=source;item.length=length;
        std::copy_n(data,length,item.data.begin());++count;
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
    if(pitchActive&&samples>0) {
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
        for(int start=0;start<samples;start+=int(audioScratch.size())) {
            const int n=std::min(int(audioScratch.size()),samples-start);
            for(int i=0;i<n;++i) {
                float value=channels?clean(input.getSample(0,start+i)):0.0f;
                // Sanitize each channel before mixing: NaN/Inf in one side
                // must not erase a valid signal in the other side.
                if(channels>1&&!antiPhase)value=.5f*value+.5f*clean(input.getSample(1,start+i));
                audioScratch[size_t(i)]=value;
            }
            detectedEvents.clear();detector.process(audioScratch.data(),n,detectedEvents);
            for(size_t i=0;i<detectedEvents.size;++i) {
                const auto& event=detectedEvents.events[i];
                const juce::uint8 raw[]{juce::uint8(event.on?0x90:0x80),juce::uint8(event.note),juce::uint8(event.velocity)};
                append(start+event.offset,raw,3,flysnapper::Source::Generated);
            }
            if(detectedEvents.overflow){inputOverflow=true;break;}
        }
        const auto status=detector.status();
        inputLevel.store(status.levelDb,std::memory_order_relaxed);
        detectedMask.store(status.activePitchClasses,std::memory_order_relaxed);
    } else {inputLevel.store(-120.0f,std::memory_order_relaxed);detectedMask.store(0,std::memory_order_relaxed);}
    if(inputOverflow) {
        engine.panic(0,output);engine.clearChord();testRemaining=-1;dropped.fetch_add(1);count=0;
        detectedEvents.clear();detector.reset(detectedEvents,0);detectedMask.store(0);
    }
    std::sort(inputs.begin(),inputs.begin()+std::ptrdiff_t(count),[](const auto& a,const auto& b){
        return a.offset<b.offset||(a.offset==b.offset&&a.sequence<b.sequence);
    });
    unsigned noteCount=0;
    auto drain=[&] { for(size_t k=0;k<output.size;++k) {
        const auto& e=output.events[k];
        const int command=e.type==flysnapper::EventType::NoteOn?0x90:e.type==flysnapper::EventType::NoteOff?0x80:0xb0;
        const juce::uint8 raw[]{juce::uint8(command|(e.channel-1)),juce::uint8(e.note),juce::uint8(e.value)};
        result.addEvent(raw,3,e.offset);if(e.type==flysnapper::EventType::NoteOn)++noteCount;
    } output.size=0; };
    drain();
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
            if(status==0x90&&e.length==3&&e.data[2]>0)engine.noteOn(e.source,ch,e.data[1],e.data[2],e.offset,output);
            else if((status==0x80||status==0x90)&&e.length==3)engine.noteOff(e.source,ch,e.data[1],e.data[2],e.offset,output);
            else if(status==0xb0&&e.length==3)engine.controller(e.source,ch,e.data[1],e.data[2],e.offset,output);
            else result.addEvent(e.data.data(),e.length,e.offset); // Channel pressure, bend, etc. preserve as supplied by adapter.
            drain();
        }
        drain();i=end;
    }
    if(output.overflow)dropped.fetch_add(1);
    faulted.store(engine.overloaded(),std::memory_order_relaxed);
    emitted.fetch_add(noteCount,std::memory_order_relaxed);
    display.store(flysnapper::packSnapshot(engine.snapshot()),std::memory_order_release);
    // Retain our preallocated output buffer instead of swapping in an
    // arbitrary host buffer. The adapter owns/resizes its delivery buffer.
    audio.clear();midi.clear();midi.addEvents(result,0,-1,0);
}
juce::AudioProcessorEditor* FlysnapperProcessor::createEditor(){return new FlysnapperEditor(*this);}
void FlysnapperProcessor::getStateInformation(juce::MemoryBlock& out) {
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
    auto set=[&](const juce::String& id,int value) {
        for(auto child:values)if(child.getProperty("id").toString()==id){child.setProperty("value",value,nullptr);return;}
        juce::ValueTree child("PARAM");child.setProperty("id",id,nullptr);child.setProperty("value",value,nullptr);values.addChild(child,-1,nullptr);
    };
    set("routingTest",0);
    // Stable string IDs are authoritative. Unknown/new types retain defaults.
    const auto defaults=flysnapper::factoryDefaults();for(int i=0;i<50;++i)set(mappingParameterId(i),defaults[size_t(i)]);
    if(auto* table=state->getChildByName("ChordScales"))for(auto* item:table->getChildIterator()) {
        int chord=flysnapper::chordIndexFromId(item->getStringAttribute("chord").toStdString());
        int s=flysnapper::scaleIndexFromId(item->getStringAttribute("scale").toStdString());
        if(chord>=0&&s>=0)set(mappingParameterId(chord),s);
    }
    const int s=flysnapper::scaleIndexFromId(state->getStringAttribute("fixedScale").toStdString());if(s>=0)set("scale",s);
    parameters.replaceState(values);testRequested.store(false);needsReset.store(true);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FlysnapperProcessor;}
