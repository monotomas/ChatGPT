// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace {
int discreteChoice(float value,int last) noexcept {
    // APVTS deliberately leaves its initial raw default unsnapped. With38
    // choices, e.g.3/37*37 can be2.999999762f even though the parameter's
    // discrete choice is3. Host changes normally snap, but initial reads and
    // ID-based state export must use the same nearest-choice interpretation.
    if(!std::isfinite(value))return 0;
    return juce::roundToInt(std::clamp(value,0.0f,float(last)));
}
}

juce::String FlysnapperProcessor::mappingParameterId(int type) {
    return "map_"+juce::String(flysnapper::chordTypes()[size_t(type)].id);
}
juce::AudioProcessorValueTreeState::ParameterLayout FlysnapperProcessor::parameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"correction",1},"Note Correction",true));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},"Mode",
        juce::StringArray{"Chord Tones Only","Chord-Scale Assignment","Fixed Scale"},2));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"root",1},"Root",
        juce::StringArray{"C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"},0));
    juce::StringArray scales;for(const auto& s:flysnapper::scaleDefinitions())scales.add(juce::String::fromUTF8(s.name));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scale",1},"Scale",scales,0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"chordThru",1},"Chord MIDI Through",false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"routingTest",1},"Routing Test",false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"help",1},"Context Help",true));
    for(int i=0;i<flysnapper::chordTypeCount;++i) {
        const auto& c=flysnapper::chordTypes()[size_t(i)];
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{mappingParameterId(i),1},
            "Map "+juce::String(c.id),scales,c.defaultScale));
    }
    return p;
}
FlysnapperProcessor::FlysnapperProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      parameters(*this,nullptr,"FLYSNAPPER_PARAMETERS",parameterLayout()) {
    correction=parameters.getRawParameterValue("correction");mode=parameters.getRawParameterValue("mode");
    root=parameters.getRawParameterValue("root");scale=parameters.getRawParameterValue("scale");
    chordThru=parameters.getRawParameterValue("chordThru");routingTest=parameters.getRawParameterValue("routingTest");
    for(int i=0;i<flysnapper::chordTypeCount;++i)mapValues[size_t(i)]=parameters.getRawParameterValue(mappingParameterId(i));
    // Bounded raw passthrough, engine output and chord resynchronisation fit
    // this buffer without an audio-thread allocation. Delivery is host-owned.
    result.ensureSize(262144);
    flysnapper::Settings initial;initial.mode=flysnapper::Mode::FixedScale;initial.scale=0;
    engine.setSettings(initial);display.store(flysnapper::packSnapshot(engine.snapshot()));setLatencySamples(0);
}
bool FlysnapperProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    return layout.inputBuses.isEmpty()&&layout.outputBuses.size()==1
        &&layout.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
}
void FlysnapperProcessor::prepareToPlay(double rate,int) {
    sampleRate=std::isfinite(rate)&&rate>=8000&&rate<=384000?rate:48000;
    result.ensureSize(262144);needsReset.store(true,std::memory_order_release);
    testRemaining=-1;testWasHigh=false;wasPlaying=false;havePosition=false;
}
void FlysnapperProcessor::releaseResources() {needsReset.store(true,std::memory_order_release);testRemaining=-1;}
void FlysnapperProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){process(audio,midi,false);}
void FlysnapperProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){process(audio,midi,true);}

void FlysnapperProcessor::process(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi,bool bypassed) {
    const int samples=audio.getNumSamples();result.clear();output.clear();unsigned noteCount=0;
    const auto raw=[&](const juce::uint8* bytes,int length,int offset) {
        result.addEvent(bytes,length,offset);
        if(length==3&&(bytes[0]&0xf0)==0x90&&bytes[2]>0)++noteCount;
    };
    const auto message=[&](int status,int a,int b,int offset) {
        const juce::uint8 bytes[]{juce::uint8(status),juce::uint8(a),juce::uint8(b)};raw(bytes,3,offset);
    };
    const auto drain=[&] {
        for(size_t i=0;i<output.size;++i) {
            const auto& e=output.events[i];
            // The correction engine owns only channel1. Overload cleanup of
            // other playing/chord channels is the adapter's responsibility.
            if(e.channel!=1)continue;
            const int command=e.type==flysnapper::EventType::NoteOn?0x90:e.type==flysnapper::EventType::NoteOff?0x80:0xb0;
            message(command,e.note,e.value,e.offset);
        }
        output.clear();
    };
    const auto clearChord=[&](int offset,bool relay) {
        engine.clearChord();chordHeld={};chordVelocity={};chordHeldCount=0;
        if(relay)message(0xbf,123,0,offset);
    };
    const auto clearPassthrough=[&](int offset) {
        for(int ch=2;ch<=15;++ch) {
            auto& notes=passthroughHeld[size_t(ch-1)];
            for(int n=0;n<128;++n)for(unsigned k=0;k<notes[size_t(n)];++k)message(0x80|(ch-1),n,0,offset);
            const auto holds=passthroughHolds[size_t(ch-1)];
            if(holds&1)message(0xb0|(ch-1),64,0,offset);
            if(holds&2)message(0xb0|(ch-1),66,0,offset);
            if(holds&4)message(0xb0|(ch-1),69,0,offset);
        }
        passthroughHeld={};passthroughHolds={};passthroughHeldCount=0;
    };
    bool relayClearedThisBlock=false;
    bool resetNow=needsReset.exchange(false,std::memory_order_acq_rel);
    if(auto* head=getPlayHead())if(auto position=head->getPosition()) {
        const bool playing=position->getIsPlaying();if(wasPlaying&&!playing)resetNow=true;
        if(auto time=position->getTimeInSamples()) {
            if(playing&&wasPlaying&&havePosition&&*time!=expectedPosition)resetNow=true;
            expectedPosition=*time+samples;havePosition=true;
        } else havePosition=false;
        wasPlaying=playing;
    }
    if(resetNow) {
        engine.panic(0,output);drain();clearPassthrough(0);clearChord(0,previousChordThru);
        relayClearedThisBlock=previousChordThru;
        ioFault=false;testRemaining=-1;
    }
    flysnapper::Settings settings;
    settings.correctionEnabled=correction->load(std::memory_order_relaxed)>=.5f&&!bypassed;
    settings.mode=static_cast<flysnapper::Mode>(discreteChoice(mode->load(std::memory_order_relaxed),2));
    settings.root=discreteChoice(root->load(std::memory_order_relaxed),11);
    settings.scale=discreteChoice(scale->load(std::memory_order_relaxed),flysnapper::scaleCount-1);
    for(int i=0;i<flysnapper::chordTypeCount;++i)
        settings.chordScales[size_t(i)]=discreteChoice(mapValues[size_t(i)]->load(std::memory_order_relaxed),flysnapper::scaleCount-1);
    engine.setSettings(settings);
    const bool through=chordThru->load(std::memory_order_relaxed)>=.5f;
    if(through!=previousChordThru) {
        if(previousChordThru&&!relayClearedThisBlock)message(0xbf,123,0,0);
        else if(chordHeldCount>0) {
            // Route changes replay actual held pitches/counts, including
            // overlapping unisons, rather than guessing from pitch classes.
            message(0xbf,123,0,0);
            for(int n=0;n<128;++n)for(unsigned k=0;k<chordHeld[size_t(n)];++k)
                message(0x9f,n,chordVelocity[size_t(n)],0);
        }
        previousChordThru=through;
    }
    size_t count=0,payloadBytes=0;bool overflow=false;
    const auto append=[&](int offset,const juce::uint8* bytes,int length,flysnapper::Source source) {
        if(length<1||count==inputs.size()||size_t(length)>maximumInputBytes-payloadBytes){overflow=true;return;}
        auto& item=inputs[count];item={bytes,length,std::clamp(offset,0,std::max(0,samples-1)),unsigned(count),source};
        payloadBytes+=size_t(length);++count;
    };
    for(const auto e:midi) {
        append(e.samplePosition,e.data,e.numBytes,flysnapper::Source::External);
        if(overflow)break;
    }
    const bool testHigh=routingTest->load(std::memory_order_relaxed)>=.5f;
    const bool button=testRequested.exchange(false,std::memory_order_acq_rel);
    const bool testStart=(testHigh&&!testWasHigh)||button;
    if((!testHigh&&testWasHigh)||bypassed){engine.stopSource(flysnapper::Source::Diagnostic,0,output);drain();testRemaining=-1;}
    testWasHigh=testHigh;
    const juce::uint8 diagnosticOn[]{0x90,60,100},diagnosticOff[]{0x80,60,0};
    if(samples>0&&!bypassed&&testStart) {
        engine.stopSource(flysnapper::Source::Diagnostic,0,output);drain();
        append(0,diagnosticOn,3,flysnapper::Source::Diagnostic);testRemaining=std::max<int64_t>(1,std::llround(sampleRate*.1));
    }
    if(samples>0&&testRemaining>=0) {
        if(testRemaining<samples){append(int(testRemaining),diagnosticOff,3,flysnapper::Source::Diagnostic);testRemaining=-1;}
        else testRemaining-=samples;
    }
    const auto failSafe=[&](int offset) {
        // Pending Ons may not have been delivered. Replace partial output
        // with bounded receiver cleanup and require explicit reset/Panic.
        result.clear();noteCount=0;engine.panic(offset,output);output.clear();
        for(int ch=1;ch<=15;++ch) {
            message(0xb0|(ch-1),120,0,offset);message(0xb0|(ch-1),123,0,offset);
            for(int cc:{64,66,69})message(0xb0|(ch-1),cc,0,offset);
        }
        passthroughHeld={};passthroughHolds={};passthroughHeldCount=0;
        clearChord(offset,through||previousChordThru);testRemaining=-1;ioFault=true;
        dropped.fetch_add(1,std::memory_order_relaxed);
    };
    if(overflow)failSafe(0);
    const auto validChannelMessage=[](const Input& e) {
        const int status=e.data[0]&0xf0;
        const int expected=status==0xc0||status==0xd0?2:3;
        return e.length==expected&&e.data[1]<128&&(expected==2||e.data[2]<128);
    };
    if(!ioFault) {
        std::sort(inputs.begin(),inputs.begin()+std::ptrdiff_t(count),[](const Input& a,const Input& b){
            return a.offset<b.offset||(a.offset==b.offset&&a.sequence<b.sequence);
        });
        for(size_t i=0;i<count&&!ioFault;) {
            size_t end=i+1;while(end<count&&inputs[end].offset==inputs[i].offset)++end;
            // Playing onsets sharing this exact sample are classified within
            // their own source. A routing-test pulse cannot erase the player's
            // melodic history. Other MIDI channels do not enter this engine.
            std::array<int,3> ons{};
            for(size_t j=i;j<end;++j) {
                const auto& e=inputs[j];const int status=e.data[0]&0xf0;
                if(e.source==flysnapper::Source::External&&e.data[0]>=0x80&&e.data[0]<0xf0&&(e.data[0]&15)==15) {
                    if(!validChannelMessage(e)){dropped.fetch_add(1,std::memory_order_relaxed);continue;}
                    if(status==0x90||status==0x80) {
                        auto& held=chordHeld[size_t(e.data[1])];
                        if(status==0x90&&e.data[2]>0) {
                            if(chordHeldCount==maximumHeldInputs){failSafe(e.offset);break;}
                            ++held;++chordHeldCount;chordVelocity[size_t(e.data[1])]=e.data[2];engine.chordNoteOn(e.data[1],e.data[2]);
                        } else {
                            if(held){--held;--chordHeldCount;}engine.chordNoteOff(e.data[1]);
                        }
                    } else if(status==0xb0&&(e.data[1]==120||e.data[1]==123)) {
                        engine.clearChord();chordHeld={};chordVelocity={};chordHeldCount=0;
                    }
                    if(through)raw(e.data,e.length,e.offset);
                } else if(e.data[0]>=0x80&&e.data[0]<0xf0&&validChannelMessage(e)
                    &&status==0x90&&(e.data[0]&15)==0&&e.data[2]>0)++ons[size_t(e.source)];
            }
            if(ioFault)break;
            engine.refreshChord();
            for(size_t stream=0;stream<ons.size();++stream)
                engine.beginTimestamp(static_cast<flysnapper::Source>(stream),1,ons[stream]);
            for(size_t j=i;j<end&&!ioFault;++j) {
                const auto& e=inputs[j];const int status=e.data[0]&0xf0;
                const bool channel=e.data[0]>=0x80&&e.data[0]<0xf0;const int ch=(e.data[0]&15)+1;
                if(e.source==flysnapper::Source::External&&channel&&ch==16)continue;
                if(channel&&!validChannelMessage(e)) {
                    dropped.fetch_add(1,std::memory_order_relaxed);continue;
                }
                if(channel&&ch==1) {
                    if(status==0x90&&e.length==3&&e.data[2]>0)engine.noteOn(e.source,1,e.data[1],e.data[2],e.offset,output);
                    else if((status==0x80||status==0x90)&&e.length==3)engine.noteOff(e.source,1,e.data[1],e.data[2],e.offset,output);
                    else if(status==0xb0&&e.length==3)engine.controller(e.source,1,e.data[1],e.data[2],e.offset,output);
                    else raw(e.data,e.length,e.offset);
                    if(engine.overloaded()||output.overflow){failSafe(e.offset);break;}drain();
                } else {
                    if(channel&&ch>=2&&ch<=15&&e.length==3) {
                        auto& notes=passthroughHeld[size_t(ch-1)];
                        if(status==0x90&&e.data[2]>0) {
                            if(passthroughHeldCount==maximumHeldInputs){failSafe(e.offset);break;}
                            ++notes[size_t(e.data[1])];++passthroughHeldCount;
                        } else if(status==0x80||status==0x90) {
                            if(notes[size_t(e.data[1])]){--notes[size_t(e.data[1])];--passthroughHeldCount;}
                        } else if(status==0xb0) {
                            auto& holds=passthroughHolds[size_t(ch-1)];
                            if(e.data[1]==120||e.data[1]==123) {
                                for(auto n:notes)passthroughHeldCount-=n;notes={};holds=0;
                            } else if(e.data[1]==121)holds=0;
                            else if(e.data[1]==64||e.data[1]==66||e.data[1]==69) {
                                const uint8_t bit=uint8_t(e.data[1]==64?1:e.data[1]==66?2:4);
                                holds=e.data[2]>=64?uint8_t(holds|bit):uint8_t(holds&~bit);
                            }
                        }
                    }
                    raw(e.data,e.length,e.offset);
                }
            }
            i=end;
        }
    }
    faulted.store(ioFault||engine.overloaded(),std::memory_order_relaxed);
    emitted.fetch_add(noteCount,std::memory_order_relaxed);emittedEvents.fetch_add(unsigned(result.getNumEvents()),std::memory_order_relaxed);
    display.store(flysnapper::packSnapshot(engine.snapshot()),std::memory_order_release);
    audio.clear();midi.clear();midi.addEvents(result,0,-1,0);
}

juce::AudioProcessorEditor* FlysnapperProcessor::createEditor(){return new FlysnapperEditor(*this);}
void FlysnapperProcessor::getStateInformation(juce::MemoryBlock& destination) {
    juce::XmlElement state("FLYSNAPPER_STATE");state.setAttribute("schema",1);
    auto values=parameters.copyState();
    for(auto child:values)if(child.getProperty("id").toString()=="routingTest")child.setProperty("value",0,nullptr);
    const auto savedScale=[&](const juce::String& id,int fallback) {
        for(auto child:values)if(child.getProperty("id").toString()==id) {
            const float value=float(child.getProperty("value"));
            if(std::isfinite(value))return discreteChoice(value,flysnapper::scaleCount-1);
        }
        return fallback;
    };
    state.addChildElement(values.createXml().release());auto* table=state.createNewChildElement("ChordScales");
    for(int i=0;i<flysnapper::chordTypeCount;++i) {
        auto* choice=table->createNewChildElement("Choice");choice->setAttribute("chord",flysnapper::chordTypes()[size_t(i)].id);
        const int index=savedScale(mappingParameterId(i),flysnapper::chordTypes()[size_t(i)].defaultScale);
        choice->setAttribute("scale",flysnapper::scaleDefinitions()[size_t(index)].id);
    }
    const int index=savedScale("scale",0);
    state.setAttribute("fixedScale",flysnapper::scaleDefinitions()[size_t(index)].id);copyXmlToBinary(state,destination);
}
void FlysnapperProcessor::setStateInformation(const void* data,int bytes) {
    auto state=getXmlFromBinary(data,bytes);
    if(!state||!state->hasTagName("FLYSNAPPER_STATE")||state->getIntAttribute("schema")!=1)return;
    auto* xml=state->getChildByName("FLYSNAPPER_PARAMETERS");if(!xml)return;
    const auto old=juce::ValueTree::fromXml(*xml);if(!old.isValid())return;
    juce::ValueTree values("FLYSNAPPER_PARAMETERS");values.copyPropertiesFrom(old,nullptr);
    // Restore current IDs only. Old pitch/audio state cannot revive hidden DSP.
    for(auto* base:getParameters())if(auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(base)) {
        float value=parameter->convertFrom0to1(parameter->getDefaultValue());
        for(auto child:old)if(child.getProperty("id").toString()==parameter->paramID) {
            const float supplied=float(child.getProperty("value"));if(std::isfinite(supplied))value=supplied;break;
        }
        const auto& range=parameter->getNormalisableRange();value=range.snapToLegalValue(std::clamp(value,range.start,range.end));
        juce::ValueTree child("PARAM");child.setProperty("id",parameter->paramID,nullptr);child.setProperty("value",value,nullptr);values.addChild(child,-1,nullptr);
    }
    const auto set=[&](const juce::String& id,float value) {
        for(auto child:values)if(child.getProperty("id").toString()==id){child.setProperty("value",value,nullptr);return;}
    };
    set("routingTest",0);
    if(auto* table=state->getChildByName("ChordScales")) {
        const auto defaults=flysnapper::factoryDefaults();for(int i=0;i<flysnapper::chordTypeCount;++i)set(mappingParameterId(i),float(defaults[size_t(i)]));
        for(auto* item:table->getChildIterator()) {
            const int chord=flysnapper::chordIndexFromId(item->getStringAttribute("chord").toStdString());
            const int choice=flysnapper::scaleIndexFromId(item->getStringAttribute("scale").toStdString());
            if(chord>=0&&choice>=0)set(mappingParameterId(chord),float(choice));
        }
    }
    const int fixed=flysnapper::scaleIndexFromId(state->getStringAttribute("fixedScale").toStdString());if(fixed>=0)set("scale",float(fixed));
    parameters.replaceState(values);testRequested.store(false,std::memory_order_release);needsReset.store(true,std::memory_order_release);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FlysnapperProcessor;}
