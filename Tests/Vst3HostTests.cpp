// SPDX-License-Identifier: AGPL-3.0-or-later
// Actual compiled VST3 integration test. Derived from Buffkiller's JUCE host
// pattern; the assertions below exercise Flysnapper, not the reference plugin.
// No DAW or audio driver is used. These results do not establish Live behavior.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <algorithm>
#include <array>
#include <functional>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int assertions = 0;
void check(bool ok, const std::string& message) {
    ++assertions;
    if (!ok) throw std::runtime_error(message);
}

juce::AudioProcessorParameter& parameter(juce::AudioPluginInstance& plugin, const juce::String& name) {
    for (auto* p : plugin.getParameters()) if (p->getName(128) == name) return *p;
    throw std::runtime_error("Actual VST3 parameter not found: " + name.toStdString());
}
void setNormalised(juce::AudioPluginInstance& plugin, const juce::String& name, float value) {
    auto& p = parameter(plugin, name);
    p.beginChangeGesture(); p.setValueNotifyingHost(value); p.endChangeGesture();
}

class Timeline final : public juce::AudioPlayHead {
public:
    int64_t sample = 0;
    double rate = 48000;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p;
        p.setIsPlaying(playing); p.setTimeInSamples(sample);
        p.setBpm(120); p.setPpqPosition(double(sample) * 2 / rate);
        return p;
    }
};
struct Note {
    bool on; int channel; int pitch; int velocity; int offset;
};
std::vector<Note> notes(const juce::MidiBuffer& midi) {
    std::vector<Note> result;
    for (const auto metadata : midi) {
        auto message = metadata.getMessage();
        if (message.isNoteOnOrOff()) result.push_back({message.isNoteOn(), message.getChannel(),
            message.getNoteNumber(), message.getVelocity(), metadata.samplePosition});
    }
    return result;
}
void expectNote(const std::vector<Note>& output, size_t index, bool on, int pitch,
                int offset, int channel = 1, int velocity = -1) {
    check(index < output.size(), "Missing output MIDI note at index " + std::to_string(index));
    const auto& n = output[index];
    check(n.on == on && n.pitch == pitch && n.offset == offset && n.channel == channel
          && (velocity < 0 || n.velocity == velocity),
          "Actual VST3 MIDI mismatch at index " + std::to_string(index) + ": got "
          + (n.on ? "on " : "off ") + std::to_string(n.pitch) + " @ "
          + std::to_string(n.offset) + " channel " + std::to_string(n.channel));
}
void chord(juce::MidiBuffer& midi, bool on, std::initializer_list<int> pitches, int offset) {
    for (int p : pitches) midi.addEvent(on ? juce::MidiMessage::noteOn(16, p, juce::uint8(100))
                                        : juce::MidiMessage::noteOff(16, p), offset);
}

class Session {
public:
    Session(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& description,
            double rate = 48000, int block = 128) : maxBlock(block) {
        juce::String error;
        plugin = formats.createPluginInstance(description, rate, block, error);
        if (!plugin) throw std::runtime_error("Actual VST3 creation failed: " + error.toStdString());
        // Musical routing fixtures select their own policy; product defaults
        // intentionally start in playable Fixed Scale / Chromatic.
        setNormalised(*plugin, "Mode", 0);
        setNormalised(*plugin, "Pitch Enabled", 0);
        timeline.rate = rate;
        plugin->setPlayHead(&timeline);
        plugin->prepareToPlay(rate, block);
    }
    ~Session() { plugin->setPlayHead(nullptr); plugin->releaseResources(); }
    std::vector<Note> process(juce::MidiBuffer& midi, int count = -1, bool sidechainSignal = false) {
        if (count < 0) count = maxBlock;
        juce::AudioBuffer<float> audio(std::max(2, std::max(plugin->getTotalNumInputChannels(),
                                                          plugin->getTotalNumOutputChannels())), count);
        audio.clear();
        if (audioSource) {
            for (int c = 0; c < plugin->getTotalNumInputChannels(); ++c)
                for (int n = 0; n < count; ++n)
                    audio.setSample(c, n, audioSource(timeline.sample + n));
        } else if (sidechainSignal) {
            for (int c = 0; c < plugin->getTotalNumInputChannels(); ++c)
                for (int n = 0; n < count; ++n)
                    audio.setSample(c, n, float(.1 * std::sin(6.283185307179586 * 440
                        * double(timeline.sample + n) / timeline.rate)));
        }
        plugin->processBlock(audio, midi);
        for (int c = 0; c < plugin->getTotalNumOutputChannels(); ++c)
            for (int n = 0; n < count; ++n)
                check(std::isfinite(audio.getSample(c, n)) && audio.getSample(c, n) == 0,
                      "Instrument audio output must be silent; source monitoring belongs to source track");
        timeline.sample += count;
        return notes(midi);
    }
    void flushControls() {
        juce::MidiBuffer midi;
        process(midi);
    }
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    Timeline timeline;
    int maxBlock;
    std::function<float(int64_t)> audioSource;
};

void testRouting(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& description,
                 double rate, int block) {
    Session session(formats, description, rate, block);
    auto& plugin = *session.plugin;
    check(plugin.acceptsMidi(), "Actual VST3 must accept MIDI");
    check(plugin.producesMidi(), "Actual VST3 must expose MIDI output");
    check(plugin.hasEditor(), "Actual VST3 must expose its editor");
    check(plugin.getBusCount(false) >= 1 && plugin.getChannelCountOfBus(false, 0) == 2,
          "Actual VST3 must expose stereo instrument output");
    check(plugin.getLatencySamples() == 0, "MIDI routing unexpectedly reports frame compensation latency");

    juce::MidiBuffer midi;
    // Deliberately insert performance input first at the same sample: chord
    // controls are applied before performance notes regardless of event order.
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(93)), 11);
    chord(midi, true, {48, 52, 55}, 11);
    midi.addEvent(juce::MidiMessage::noteOff(1, 61), block - 3);
    auto output = session.process(midi);
    check(output.size() == 2, "Chord control notes leaked or performance note missing");
    expectNote(output, 0, true, 60, 11, 1, 93);
    expectNote(output, 1, false, 60, block - 3);

    // Register-independent correction and channel preservation.
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(3, 13, juce::uint8(75)), 1);
    midi.addEvent(juce::MidiMessage::noteOn(4, 109, juce::uint8(88)), 2);
    midi.addEvent(juce::MidiMessage::noteOff(3, 13), 3);
    midi.addEvent(juce::MidiMessage::noteOff(4, 109), 4);
    output = session.process(midi);
    check(output.size() == 4, "Register/channel test event count");
    expectNote(output, 0, true, 12, 1, 3, 75);
    expectNote(output, 1, true, 108, 2, 4, 88);
    expectNote(output, 2, false, 12, 3, 3);
    expectNote(output, 3, false, 108, 4, 4);

    // A chord change in the middle of the block must not affect earlier notes,
    // and an old input's off must end the original output after that change.
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(100)), 5);
    chord(midi, false, {48, 52, 55}, block / 2);
    chord(midi, true, {50, 53, 57}, block / 2);
    midi.addEvent(juce::MidiMessage::noteOn(1, 63, juce::uint8(101)), block / 2);
    midi.addEvent(juce::MidiMessage::noteOff(1, 61), block - 2);
    midi.addEvent(juce::MidiMessage::noteOff(1, 63), block - 1);
    output = session.process(midi);
    check(output.size() == 4, "Mid-block chord change event count");
    expectNote(output, 0, true, 60, 5);
    expectNote(output, 1, true, 62, block / 2);
    expectNote(output, 2, false, 60, block - 2);
    expectNote(output, 3, false, 62, block - 1);

    // A zero-velocity Note On is a Note Off throughout the compiled wrapper.
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 65, juce::uint8(111)), 7);
    midi.addEvent(juce::MidiMessage::noteOn(1, 65, juce::uint8(0)), 19);
    output = session.process(midi);
    check(output.size() == 2, "Velocity-zero lifecycle event count");
    expectNote(output, 0, true, 65, 7);
    expectNote(output, 1, false, 65, 19);

    std::cout << "PASS actual VST3 MIDI/chord roles, register, velocity, original Note Off and offsets: "
              << rate << " Hz / " << block << " samples\n";
}

void testSidechain(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& description) {
    Session session(formats, description);
    auto& p = *session.plugin;
    int sidechain = -1;
    for (int b = 0; b < p.getBusCount(true); ++b)
        if (p.getBus(true, b)->getName().containsIgnoreCase("sidechain")) sidechain = b;
    check(sidechain >= 0, "Actual VST3 sidechain bus not found");
    // Inspect the SDK component, not JUCE's first-input abstraction: Live
    // needs an actual kAux audio bus to expose its native sidechain panel.
    const auto* extension = p.getVST3Client();
    check(extension != nullptr, "VST3 SDK extension unavailable");
    auto* component = extension->getIComponentPtr();
    check(component != nullptr, "VST3 IComponent unavailable");
    check(component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput) == 1,
          "Expected one audio input, dedicated to sidechain");
    Steinberg::Vst::BusInfo info{};
    check(component->getBusInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, 0, info)
              == Steinberg::kResultOk, "VST3 audio input bus metadata unavailable");
    check(info.busType == Steinberg::Vst::kAux && info.channelCount == 2,
          "Native VST3 audio input is not a stereo kAux sidechain");
    check(component->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kInput) == 1
          && component->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput) == 1,
          "Native VST3 MIDI event input/output buses missing");
    p.releaseResources();
    check(p.getBus(true, sidechain)->enable(true), "Actual VST3 sidechain could not be activated");
    p.prepareToPlay(48000, 128);
    check(p.getBus(true, sidechain)->isEnabled(), "Sidechain activation was not retained");
    juce::MidiBuffer midi;
    chord(midi, true, {48, 52, 55}, 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(90)), 13);
    auto output = session.process(midi, 128, true);
    check(output.size() == 1, "Sidechain audio unexpectedly changed MIDI routing");
    expectNote(output, 0, true, 64, 13);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1, 64), 9);
    output = session.process(midi); // Enabled, digitally silent source.
    check(output.size() == 1, "Digitally silent enabled sidechain disrupted MIDI");
    expectNote(output, 0, false, 64, 9);
    p.releaseResources();
    check(p.getBus(true, sidechain)->enable(false), "Actual VST3 sidechain could not be disabled");
    p.prepareToPlay(48000, 128);
    check(!p.getBus(true, sidechain)->isEnabled(), "Sidechain deactivation was not retained");
    midi.clear(); chord(midi, true, {48, 52, 55}, 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(90)), 13);
    midi.addEvent(juce::MidiMessage::noteOff(1, 64), 27);
    output = session.process(midi);
    check(output.size() == 2, "External MIDI failed with disabled sidechain");
    expectNote(output, 0, true, 64, 13);
    expectNote(output, 1, false, 64, 27);
    std::cout << "PASS actual VST3 named sidechain activation, audio, digital silence and deactivation\n";
}

void testGenerated(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& description) {
    Session session(formats, description);
    setNormalised(*session.plugin, "Correction", 0);
    session.flushControls();
    setNormalised(*session.plugin, "Routing Test", 1);
    std::vector<Note> generated;
    int externalCount = 0;
    for (int start = 0; start < 5000; start += 128) {
        juce::MidiBuffer midi;
        if (start == 0) {
            midi.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(77)), 11);
            midi.addEvent(juce::MidiMessage::noteOff(1, 64), 31);
        }
        for (auto n : session.process(midi)) {
            if (n.pitch == 60) { n.offset += start; generated.push_back(n); }
            else {
                check(n.pitch == 64, "Unexpected internally generated pitch");
                if (n.on) check(n.offset == 11 && n.velocity == 77, "Simultaneous external onset changed");
                else check(n.offset == 31, "Simultaneous external release changed");
                ++externalCount;
            }
        }
    }
    check(externalCount == 2, "External MIDI did not coexist with routing-test note");
    check(generated.size() == 2, "Routing test must generate exactly one on/off pair per rising edge");
    expectNote(generated, 0, true, 60, 0, 1, 100);
    expectNote(generated, 1, false, 60, 4800);
    setNormalised(*session.plugin, "Routing Test", 0);
    session.flushControls();
    setNormalised(*session.plugin, "Routing Test", 1);
    juce::MidiBuffer midi;
    auto output = session.process(midi);
    check(output.size() == 1, "Re-arming routing test failed");
    expectNote(output, 0, true, 60, 0);
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(72)), 5);
    output = session.process(midi);
    check(output.size() == 1 && output[0].pitch == 64 && output[0].on,
          "Concurrent external sustained note was not accepted");
    setNormalised(*session.plugin, "Routing Test", 0);
    midi.clear(); output = session.process(midi);
    check(output.size() == 1, "Routing test deactivation should release only its note");
    expectNote(output, 0, false, 60, 0);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1, 64), 17);
    output = session.process(midi);
    check(output.size() == 1, "Generated-source deactivation lost external note binding");
    expectNote(output, 0, false, 64, 17);
    std::cout << "PASS actual VST3 generated/external simultaneous events, 4800-sample test gate, source-only shutdown\n";
}

void testState(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& description) {
    Session original(formats, description), restored(formats, description), independent(formats, description);
    auto& p = *original.plugin;
    auto savedControl = [](const juce::String& name) {
        return name == "Correction" || name == "Mode" || name == "Root" || name == "Scale"
            || name == "Pitch Enabled" || name == "Pitch Mode" || name == "Input Threshold"
            || name == "Sensitivity" || name == "Release" || name.startsWith("Map ");
    };
    struct SavedValue {juce::String name; float original; float independent;};
    std::vector<SavedValue> expected;
    int index = 0;
    for (auto* item : p.getParameters()) {
        const auto name = item->getName(128);
        if (!savedControl(name)) continue;
        // The VST3 host caches the precise normalized value supplied by the
        // host, whereas AudioParameterChoice rounds to a legal choice before
        // state serialization. Exercise actual choices, and compare against
        // the intended value rather than an arbitrary between-choice cache.
        float value=0;
        if(name=="Input Threshold")value=item->getValueForText("-61");
        else if(name=="Sensitivity")value=item->getValueForText("0.73");
        else if(name=="Release")value=item->getValueForText("48");
        else {
            const int steps=item->getNumSteps();
            check(steps>=2&&steps<=19,"Unexpected musical parameter choice range");
            value=float((index*7)%steps)/float(steps-1);
        }
        ++index;
        check(value>=0&&value<=1,"Host failed to parse legal audio parameter value");
        item->setValueNotifyingHost(value);
        expected.push_back({name,value,parameter(*independent.plugin,name).getValue()});
    }
    check(index == 59, "Expected nine musical/audio controls and exactly 50 chord-scale parameters");
    original.flushControls();
    for (const auto& saved : expected)
        check(std::abs(parameter(p,saved.name).getValue() - saved.original) < 0.00001f,
              "Source instance did not accept legal parameter choice: " + saved.name.toStdString());
    juce::MemoryBlock state;
    p.getStateInformation(state);
    check(state.getSize() > 0, "Actual VST3 state was empty");
    restored.plugin->setStateInformation(state.getData(), int(state.getSize()));
    restored.flushControls();
    for (const auto& saved : expected) {
        check(std::abs(parameter(*restored.plugin,saved.name).getValue() - saved.original) < 0.00001f,
              "Actual VST3 state roundtrip failed: " + saved.name.toStdString() + " expected="
              + std::to_string(saved.original) + " actual="
              + std::to_string(parameter(*restored.plugin,saved.name).getValue()));
        check(parameter(*independent.plugin,saved.name).getValue() == saved.independent,
              "Changing one plugin instance changed another instance");
    }
    // State carries settings only. Saving while a generated note is held must
    // not re-arm the transient test source or revive voices after restoration.
    setNormalised(p, "Correction", 0); original.flushControls();
    setNormalised(p, "Routing Test", 1);
    juce::MidiBuffer midi;
    auto output = original.process(midi);
    check(output.size() == 1 && output[0].on && output[0].pitch == 60,
          "State active-note fixture did not start generated note");
    p.getStateInformation(state);
    restored.plugin->setStateInformation(state.getData(), int(state.getSize()));
    midi.clear(); output = restored.process(midi);
    check(output.empty(), "State restoration revived a previous active note or routing test");
    check(parameter(*restored.plugin, "Routing Test").getValue() == 0,
          "Transient routing test must restore disabled");
    std::cout << "PASS actual VST3 all settings/table state roundtrip, instance isolation, no revived notes\n";
}

void testTransport(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& description) {
    Session session(formats, description);
    juce::MidiBuffer midi;
    chord(midi, true, {48, 52, 55}, 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(80)), 17);
    auto output = session.process(midi);
    check(output.size() == 1, "Transport test onset fixture");
    session.timeline.playing = false;
    midi.clear(); output = session.process(midi);
    check(output.size() == 1, "Transport stop did not release held note");
    expectNote(output, 0, false, 60, 0);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(80)), 1);
    output = session.process(midi);
    check(output.empty(), "Transport stop did not clear stale chord controls");
    session.timeline.playing = true;
    midi.clear(); chord(midi, true, {48, 52, 55}, 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(80)), 17);
    output = session.process(midi);
    check(output.size() == 1, "Transport restart onset fixture");
    session.timeline.sample = 0; // loop/seek backwards
    midi.clear(); output = session.process(midi);
    check(output.size() == 1, "Loop/seek did not release held note");
    expectNote(output, 0, false, 60, 0);
    midi.clear(); output = session.process(midi);
    check(output.empty(), "Loop/seek emitted duplicate releases");
    std::cout << "PASS actual VST3 stop/restart/seek flush, no stale chord or repeated note offs\n";
}

void testExpression(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& description) {
    Session session(formats, description);
    std::vector<juce::MidiMessage> messages{
        juce::MidiMessage::controllerEvent(2, 1, 73),
        juce::MidiMessage::pitchWheel(2, 12345),
        juce::MidiMessage::channelPressureChange(2, 41),
        juce::MidiMessage::programChange(2, 17)
    };
    // VST3 represents most controller input through parameter changes. Test
    // the compiled wrapper's recovered values; report its timing separately.
    for (const auto& expected : messages) {
        juce::MidiBuffer midi; midi.addEvent(expected, 17);
        session.process(midi);
        check(midi.getNumEvents() == 1, "Non-note MIDI passthrough missing/duplicated message");
        const auto actual = (*midi.begin()).getMessage();
        check(actual.getRawDataSize() == expected.getRawDataSize()
              && std::equal(actual.getRawData(), actual.getRawData() + actual.getRawDataSize(), expected.getRawData()),
              "Actual VST3 changed non-note MIDI bytes");
        std::cout << "MEASURE actual VST3 " << expected.getDescription() << " offset in=17 out="
                  << (*midi.begin()).samplePosition << "\n";
    }
    std::cout << "PASS actual VST3 CC1, pitch bend, channel pressure and program-change values\n";
}

void testLatchedControllerReset(juce::AudioPluginFormatManager& formats,
                                const juce::PluginDescription& description) {
    Session session(formats, description);
    setNormalised(*session.plugin, "Correction", 0);
    session.flushControls();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(2, 66, 127), 1);
    midi.addEvent(juce::MidiMessage::controllerEvent(2, 69, 127), 3);
    midi.addEvent(juce::MidiMessage::noteOn(2, 64, juce::uint8(90)), 11);
    midi.addEvent(juce::MidiMessage::noteOff(2, 64), 31);
    auto output = session.process(midi);
    check(midi.getNumEvents() == 4 && output.size() == 2,
          "Forwarded CC66/CC69 fixture must retain both controllers and normal note lifecycle");
    expectNote(output, 0, true, 64, 11, 2, 90);
    expectNote(output, 1, false, 64, 31, 2);
    bool down66 = false, down69 = false;
    for (const auto metadata : midi) {
        const auto message = metadata.getMessage();
        if (message.isController() && message.getControllerNumber() == 66)
            down66 = message.getChannel() == 2 && message.getControllerValue() == 127;
        if (message.isController() && message.getControllerNumber() == 69)
            down69 = message.getChannel() == 2 && message.getControllerValue() == 127;
    }
    check(down66 && down69, "Actual VST3 did not forward both destination latch pedals");
    // The input note has already been released: there are no active core
    // voices, but downstream instruments may still be latched by these CCs.
    session.timeline.playing = false;
    midi.clear(); output = session.process(midi);
    check(output.empty() && midi.getNumEvents() == 2,
          "Transport stop must release remembered CC66/CC69 despite no active core voices");
    bool up66 = false, up69 = false;
    for (const auto metadata : midi) {
        const auto message = metadata.getMessage();
        check(message.isController() && message.getChannel() == 2
              && message.getControllerValue() == 0 && metadata.samplePosition == 0,
              "Latched controller reset must be an explicit value-zero event at sample zero");
        if (message.getControllerNumber() == 66) up66 = true;
        if (message.getControllerNumber() == 69) up69 = true;
    }
    check(up66 && up69, "Stop did not reset both remembered latch controllers");
    midi.clear(); session.process(midi);
    check(midi.isEmpty(), "Latched controllers were reset repeatedly after stop");
    std::cout << "PASS actual VST3 forwarded CC66/CC69 released at transport stop without active core voices\n";
}


void testAudioPipeline(juce::AudioPluginFormatManager& formats,
                       const juce::PluginDescription& description) {
    constexpr double rate=48000;
    constexpr double twoPi=6.283185307179586476925286766559;
    auto frequency=[](int note){return 440.0*std::pow(2.0,(note-69)/12.0);};
    auto enableAudio=[](Session& session,bool polyphonic){
        auto& p=*session.plugin;
        p.releaseResources();
        check(p.getBusCount(true)==1&&p.getBus(true,0)->enable(true),
              "Actual compiled VST3 audio test cannot enable auxiliary sidechain");
        p.prepareToPlay(session.timeline.rate,session.maxBlock);
        setNormalised(p,"Pitch Enabled",1);
        setNormalised(p,"Pitch Mode",polyphonic?1.0f:0.0f);
        setNormalised(p,"Correction",1);
    };
    struct AbsoluteNote {bool on;int pitch,channel;int64_t sample;};
    auto collect=[](std::vector<AbsoluteNote>& all,const std::vector<Note>& block,
                    int64_t start,std::array<int,128>& sounding){
        for(const auto& n:block) {
            check(n.channel==1,"Audio-derived output escaped performance channel 1");
            if(n.on) {
                check(n.velocity>0,"Actual VST3 generated zero-velocity Note On");
                ++sounding[size_t(n.pitch)];
            } else {
                check(sounding[size_t(n.pitch)]>0,"Actual VST3 audio pipeline emitted unmatched Note Off");
                --sounding[size_t(n.pitch)];
            }
            all.push_back({n.on,n.pitch,n.channel,start+n.offset});
        }
    };
    // This fixture sends actual audio through the compiled auxiliary VST3 bus,
    // plus simultaneous external notes and channel-16 controls. Neither source
    // is replaced by the diagnostic Routing Test parameter.
    {
        Session session(formats,description,rate,128);enableAudio(session,false);
        session.audioSource=[&](int64_t sample){
            const double t=sample/rate;if(t>=.35)return 0.0f;
            const double phase=twoPi*frequency(61)*t;
            return float(.27*std::min(1.0,t/.002)*(std::sin(phase)+.30*std::sin(2*phase)+.12*std::sin(3*phase)));
        };
        std::array<int,128> sounding{};std::vector<AbsoluteNote> all;
        while(session.timeline.sample<rate*.65) {
            int64_t start=session.timeline.sample;juce::MidiBuffer midi;
            if(start==0) {
                chord(midi,true,{48,52,55},0);
                midi.addEvent(juce::MidiMessage::noteOn(1,69,juce::uint8(88)),17);
            }
            if(start<=7200&&start+128>7200)
                midi.addEvent(juce::MidiMessage::noteOff(1,69),int(7200-start));
            collect(all,session.process(midi),start,sounding);
        }
        std::vector<AbsoluteNote> generated,external;
        for(const auto& event:all) {
            check(event.pitch==60||event.pitch==67,
                  "Compiled audio/chord correction produced unexpected note "+std::to_string(event.pitch));
            (event.pitch==60?generated:external).push_back(event);
        }
        check(generated.size()==2&&generated.front().on&&!generated.back().on,
              "Compiled mono audio must yield one corrected C# -> C lifecycle");
        check(generated.front().sample<=int64_t(rate*.15),
              "Compiled mono pipeline exceeded 150 ms functional acquisition budget");
        check(external.size()==2&&external.front().on&&external.front().sample==17
              &&!external.back().on&&external.back().sample==7200,
              "Simultaneous external A -> G correction or timing changed");
        for(int count:sounding)check(count==0,"Compiled mono audio left a held output note");
        std::cout<<"MEASURE actual VST3 audio first useful note "<<1000.*generated.front().sample/rate
                 <<" ms (functional sample observation, excludes driver/Live/synth latency)\n";
    }
    {
        Session session(formats,description,rate,128);enableAudio(session,true);
        setNormalised(*session.plugin,"Mode",1); // Fixed Scale index 2 of 3.
        setNormalised(*session.plugin,"Scale",0); // Chromatic.
        session.audioSource=[&](int64_t sample){
            const double t=sample/rate;if(t>=.50)return 0.0f;
            double value=0;for(int note:{60,64,67}) {
                const double phase=twoPi*frequency(note)*t;
                value+=.15*(std::sin(phase)+.20*std::sin(2*phase)+.08*std::sin(3*phase));
            }
            return float(std::min(1.0,t/.002)*value);
        };
        std::array<int,128> sounding{};std::array<bool,128> heard{};
        std::vector<AbsoluteNote> all;
        while(session.timeline.sample<rate*.85) {
            const int64_t start=session.timeline.sample;juce::MidiBuffer midi;
            collect(all,session.process(midi),start,sounding);
        }
        for(const auto& event:all)if(event.on) {
            check(event.pitch==60||event.pitch==64||event.pitch==67,
                  "Compiled polyphonic audio emitted unrelated pitch "+std::to_string(event.pitch));
            heard[size_t(event.pitch)]=true;
        }
        check(heard[60]&&heard[64]&&heard[67],"Compiled VST3 polyphonic audio missed C/E/G voice");
        for(int count:sounding)check(count==0,"Compiled polyphonic audio left an output note active");
    }
    {
        Session session(formats,description,rate,128);enableAudio(session,false);
        setNormalised(*session.plugin,"Mode",1);setNormalised(*session.plugin,"Scale",0);
        session.audioSource=[&](int64_t sample){
            const double t=sample/rate;const double phase=twoPi*frequency(57)*t;
            return float(.27*std::min(1.0,t/.002)*(std::sin(phase)+.25*std::sin(2*phase)));
        };
        std::array<int,128> sounding{};std::vector<AbsoluteNote> all;
        while(session.timeline.sample<12000) {
            const int64_t start=session.timeline.sample;juce::MidiBuffer midi;
            if(start==0)midi.addEvent(juce::MidiMessage::noteOn(1,72,juce::uint8(91)),0);
            collect(all,session.process(midi),start,sounding);
        }
        check(sounding[57]==1&&sounding[72]==1,"Compiled source-stop fixture lacks both real sources");
        setNormalised(*session.plugin,"Pitch Enabled",0);
        int64_t start=session.timeline.sample;juce::MidiBuffer midi;
        collect(all,session.process(midi),start,sounding);
        check(sounding[57]==0&&sounding[72]==1,
              "Disabling compiled detector failed source-only release or cut external MIDI");
        start=session.timeline.sample;midi.clear();midi.addEvent(juce::MidiMessage::noteOff(1,72),23);
        collect(all,session.process(midi),start,sounding);
        for(int count:sounding)check(count==0,"Compiled source stop left stale note ownership");
    }
    std::cout<<"PASS actual compiled VST3 mono/poly sidechain -> MIDI -> correction, simultaneous external/control MIDI and source-only shutdown\n";
}

int run(int argc, const juce::String& path) {
    try {
        juce::ScopedJuceInitialiser_GUI gui;
        check(argc == 2, "Expected the complete Flysnapper.vst3 bundle path");
        juce::File bundle(path);
        check(bundle.isDirectory(), "VST3 bundle was not found: " + path.toStdString());
        juce::AudioPluginFormatManager formats;
        auto vst = std::make_unique<juce::VST3PluginFormat>();
        auto* format = vst.get(); formats.addFormat(std::move(vst));
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format->findAllTypesForFile(descriptions, bundle.getFullPathName());
        check(descriptions.size() == 1, "Actual VST3 discovery must return exactly one class");
        check(descriptions[0]->name == "Flysnapper", "Wrong product name");
        check(descriptions[0]->isInstrument, "VST3 class must be an instrument");
        std::cout << "PASS actual VST3 discovery and instrument classification\n";
        for (double rate : {44100., 48000., 96000.})
            for (int block : {32, 128, 257}) testRouting(formats, *descriptions[0], rate, block);
        testSidechain(formats, *descriptions[0]);
        testGenerated(formats, *descriptions[0]);
        testState(formats, *descriptions[0]);
        testTransport(formats, *descriptions[0]);
        testExpression(formats, *descriptions[0]);
        testLatchedControllerReset(formats, *descriptions[0]);
        testAudioPipeline(formats, *descriptions[0]);
        std::cout << "RESULT actual VST3 host tests passed (" << assertions << " assertions).\n";
#if JUCE_WINDOWS
        std::cout << "PLATFORM VERIFIED: Windows x64 compiled VST3 loaded and executed by native test host.\n";
#else
        std::cout << "PLATFORM VERIFIED: native Linux VST3; Windows build is a separate required result.\n";
#endif
        std::cout << "NOT VERIFIED BY THIS TEST: Ableton Live, audio-driver/end-to-end latency or general musical accuracy.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
} // namespace

// MSVC delivers the original UTF-16 path, including non-ASCII directory names.
#if JUCE_WINDOWS
int wmain(int argc, wchar_t** argv) {
    return run(argc, argc == 2 ? juce::String(argv[1]) : juce::String());
}
#else
int main(int argc, char** argv) {
    return run(argc, argc == 2 ? juce::String::fromUTF8(argv[1]) : juce::String());
}
#endif
