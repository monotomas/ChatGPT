// SPDX-License-Identifier: AGPL-3.0-or-later
// Execute the compiled module through JUCE's VST3 host and inspect SDK buses.
// This does not establish any Ableton Live routing or device-off behavior.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int expectedScaleCount = 38;
int assertions = 0, eventRows = 0;
int64_t silentOutputSamplesChecked = 0;
unsigned bypassSilenceCallbacks = 0;
juce::Array<juce::var> cases;
juce::StringArray features;
juce::String currentCase;
juce::String csv = "case,rate,block,sample,type,channel,note,value,offset,rawhex,stage\n";
void check(bool ok, const std::string& message) {
    ++assertions;
    if (!ok) throw std::runtime_error(message);
}
juce::AudioProcessorParameter* findParameter(juce::AudioPluginInstance& p, const juce::String& name) {
    for (auto* item : p.getParameters()) if (item->getName(128) == name) return item;
    return nullptr;
}
juce::AudioProcessorParameter& parameter(juce::AudioPluginInstance& p, const juce::String& name) {
    if (auto* item = findParameter(p, name)) return *item;
    throw std::runtime_error("Actual VST3 parameter not found: " + name.toStdString());
}
void setNormalised(juce::AudioPluginInstance& p, const juce::String& name, float value) {
    auto& item = parameter(p, name);
    item.beginChangeGesture(); item.setValueNotifyingHost(value); item.endChangeGesture();
}
void setChoice(juce::AudioPluginInstance& p, const juce::String& name, int choice) {
    auto& item = parameter(p, name);
    check(choice >= 0 && choice < item.getNumSteps(), "Illegal test choice");
    setNormalised(p, name, float(choice) / float(item.getNumSteps() - 1));
}
void chord(juce::MidiBuffer& midi, bool on, std::initializer_list<int> pitches, int offset) {
    for (int pitch : pitches)
        midi.addEvent(on ? juce::MidiMessage::noteOn(16, pitch, juce::uint8(100))
                         : juce::MidiMessage::noteOff(16, pitch), offset);
}
struct Note { bool on; int channel, pitch, velocity, offset; };
std::vector<Note> notes(const juce::MidiBuffer& midi) {
    std::vector<Note> result;
    for (const auto metadata : midi) {
        const auto m = metadata.getMessage();
        if (m.isNoteOnOrOff()) result.push_back({m.isNoteOn(), m.getChannel(),
            m.getNoteNumber(), m.getVelocity(), metadata.samplePosition});
    }
    return result;
}
void expectNote(const std::vector<Note>& output, size_t index, bool on, int pitch,
                int offset, int channel = 1, int velocity = -1) {
    check(index < output.size(), "Missing actual VST3 MIDI note " + std::to_string(index));
    const auto& n = output[index];
    check(n.on == on && n.pitch == pitch && n.offset == offset && n.channel == channel
          && (velocity < 0 || n.velocity == velocity),
          "Actual VST3 note mismatch: got " + std::string(n.on ? "on " : "off ")
          + std::to_string(n.pitch) + " @ " + std::to_string(n.offset)
          + " channel " + std::to_string(n.channel));
}
void record(const juce::String& name, double rate, int block, int64_t start, const juce::MidiBuffer& midi) {
    for (const auto metadata : midi) {
        const auto m = metadata.getMessage();
        check(metadata.samplePosition >= 0 && metadata.samplePosition < block,
              "Actual VST3 event offset outside its callback");
        juce::String type = "O";
        int note = 0, value = 0;
        if (m.isNoteOnOrOff()) { type = m.isNoteOn() ? "N" : "F"; note = m.getNoteNumber(); value = m.getVelocity(); }
        else if (m.isController()) { type = "C"; note = m.getControllerNumber(); value = m.getControllerValue(); }
        else if (m.isPitchWheel()) { type = "P"; value = m.getPitchWheelValue(); }
        else if (m.isChannelPressure()) { type = "A"; value = m.getChannelPressureValue(); }
        else if (m.isAftertouch()) { type = "A"; note = m.getNoteNumber(); value = m.getAfterTouchValue(); }
        else if (m.isProgramChange()) value = m.getProgramChangeNumber();
        csv += currentCase + "," + juce::String(rate, 0) + "," + juce::String(block) + ","
            + juce::String(start + metadata.samplePosition) + "," + type + ","
            + juce::String(m.getChannel()) + "," + juce::String(note) + "," + juce::String(value)
            + "," + juce::String(metadata.samplePosition) + ","
            + juce::String::toHexString(m.getRawData(), m.getRawDataSize(), 0) + "," + name + "\n";
        ++eventRows;
    }
}
class Timeline final : public juce::AudioPlayHead {
public:
    int64_t sample = 0;
    double rate = 48000;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p; p.setIsPlaying(playing); p.setTimeInSamples(sample);
        p.setBpm(120); p.setPpqPosition(double(sample) * 2 / rate); return p;
    }
};
class Session {
public:
    Session(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& description,
            juce::String name, double rate = 48000, int block = 128)
        : caseName(std::move(name)), maxBlock(block) {
        juce::String error;
        plugin = formats.createPluginInstance(description, rate, block, error);
        if (!plugin) throw std::runtime_error("Actual VST3 creation failed: " + error.toStdString());
        check(plugin->getBusCount(true) == 0 && plugin->getBusCount(false) == 1,
              "Instrument VST3 must have no audio input and one audio output bus");
        check(plugin->getTotalNumInputChannels() == 0 && plugin->getTotalNumOutputChannels() == 2,
              "Instrument VST3 must expose a stereo output and no audio input");
        setChoice(*plugin, "Mode", 0);
        timeline.rate = rate; plugin->setPlayHead(&timeline); plugin->prepareToPlay(rate, block);
    }
    ~Session() { plugin->setPlayHead(nullptr); plugin->releaseResources(); }
    std::vector<Note> process(juce::MidiBuffer& midi, int count = -1, bool bypassCallback = false) {
        if (count < 0) count = maxBlock;
        // Supply actual output buffers, prefilled with nonzero sentinels.
        // These are output storage, not a plugin audio-input bus.
        juce::AudioBuffer<float> audio(2, count);
        for(int ch=0;ch<2;++ch)for(int n=0;n<count;++n)audio.setSample(ch,n,ch==0?.375f:-.625f);
        if (bypassCallback) plugin->processBlockBypassed(audio, midi);
        else plugin->processBlock(audio, midi);
        check(audio.getNumChannels() == 2 && audio.getNumSamples() == count,
              "Instrument callback changed its stereo output buffer");
        bool silent=true;for(int ch=0;ch<2;++ch)for(int n=0;n<count;++n)silent&=audio.getSample(ch,n)==0.f;
        check(silent,"Compiled VST3 did not clear every seeded output sample");
        silentOutputSamplesChecked+=int64_t(count)*2;
        if(bypassCallback)++bypassSilenceCallbacks;
        // Evidence sample counts processed callback frames; a transport seek
        // changes the playhead, not this monotonic delivery clock.
        record(caseName, timeline.rate, count, processedSamples, midi);
        processedSamples += count;
        timeline.sample += count;
        return notes(midi);
    }
    void flushControls() { juce::MidiBuffer midi; process(midi); }
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    Timeline timeline;
    juce::String caseName;
    int maxBlock;
    int64_t processedSamples = 0;
};
template <typename Fn> void runCase(const juce::String& name, Fn&& fn) {
    currentCase = name;
    auto* item = new juce::DynamicObject; item->setProperty("name", name);
    try { fn(); item->setProperty("status", "passed"); }
    catch (const std::exception& e) {
        item->setProperty("status", "failed"); item->setProperty("error", e.what());
        cases.add(juce::var(item)); throw;
    }
    cases.add(juce::var(item)); std::cout << "PASS " << name << '\n';
}

// Persisted IDs: retain the original fifty, then append the two requested
// sharp-eleventh major types. This fixture also checks actual VST3 ParamIDs.
constexpr std::array<const char*, 52> chordIds{{
    "major", "minor", "sus2", "sus4", "power", "power_seventh", "major_sixth", "minor_sixth", "six_nine",
    "dominant_seventh", "major_seventh", "minor_seventh", "minor_major_seventh", "half_diminished", "diminished_seventh",
    "dominant_sus4", "dominant_sus4_flat9", "dominant_flat5", "dominant_sharp5", "dominant_flat9", "dominant_sharp9",
    "dominant_sharp11", "dominant_flat13", "dominant_flat5_flat9", "dominant_flat5_sharp9", "dominant_sharp5_flat9",
    "dominant_sharp5_sharp9", "major_seventh_sharp5", "major_seventh_sharp9", "major_seventh_sharp11", "dominant_ninth",
    "major_ninth", "minor_ninth", "minor_major_ninth", "ninth_sus4", "ninth_sharp11", "dominant_eleventh",
    "major_eleventh", "minor_eleventh", "minor_eleventh_flat5", "dominant_thirteenth", "major_thirteenth",
    "minor_major_thirteenth", "thirteenth_flat9", "thirteenth_sharp9", "thirteenth_sus4", "thirteenth_sus4_flat9",
    "minor_thirteenth", "thirteenth_add_sharp9", "minor_thirteenth_flat5", "major_thirteenth_sharp11", "major_ninth_sharp11"
}};
// External oracle from the user's edited workbook, in stable chord-ID order.
// Canonical mapping SHA256 (not derived from the runtime Core defaults):
constexpr const char* factoryMappingSha256 = "146501dd5146f620c6718bc2be9b784380cc22eef91ec1e9dac1251c5422e814";
constexpr std::array<int, 52> userFactoryChoices{{
    1,2,8,5,12,5,1,4,1,8,7,5,4,22,14,5,19,15,16,17,16,15,16,16,16,16,
    16,20,26,7,15,7,5,3,8,15,8,1,5,22,8,1,4,35,35,8,35,5,8,32,7,7
}};
void expectUserFactoryChoices(juce::AudioPluginInstance& p, bool declaredDefaults) {
    for (size_t i = 0; i < chordIds.size(); ++i) {
        auto& mapping = parameter(p, "Map " + juce::String(chordIds[i]));
        const auto expected = float(userFactoryChoices[i]) / float(expectedScaleCount - 1);
        check(std::lround(mapping.getValue() * (expectedScaleCount - 1)) == userFactoryChoices[i]
              && std::abs(mapping.getValue() - expected) < .000001f,
              "Actual VST3 factory choice differs from the user workbook: " + std::string(chordIds[i]));
        if (declaredDefaults)
            check(std::abs(mapping.getDefaultValue() - expected) < .000001f,
                  "Actual VST3 declared default differs from the user workbook: " + std::string(chordIds[i]));
    }
}

void testModule(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    check(d.isInstrument, "Actual VST3 must be classified as an instrument");
    check(d.version == "0.4.1", "Actual VST3 must report version 0.4.1");
    const auto categories = juce::StringArray::fromTokens(d.category, "|", "");
    check(categories.size()==1 && categories.contains("Instrument"),
          "Actual VST3 metadata must contain exactly Instrument, without Fx/Tools");
    Session s(formats, d, "module_contract"); auto& p = *s.plugin;
    check(p.acceptsMidi() && p.producesMidi(), "Actual VST3 MIDI input/output unavailable");
    check(p.getLatencySamples() == 0, "Pure MIDI module declares unexpected latency frames");
    const auto* extension = p.getVST3Client(); check(extension != nullptr, "VST3 SDK extension unavailable");
    auto* component = extension->getIComponentPtr(); check(component != nullptr, "VST3 IComponent unavailable");
    check(component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput) == 0
          && component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput) == 1,
          "Actual SDK component must expose no audio input and one output bus");
    Steinberg::Vst::BusInfo audioOutput{};
    check(component->getBusInfo(Steinberg::Vst::kAudio,Steinberg::Vst::kOutput,0,audioOutput)==Steinberg::kResultOk,
          "SDK instrument audio output metadata unavailable");
    check(audioOutput.mediaType==Steinberg::Vst::kAudio&&audioOutput.direction==Steinberg::Vst::kOutput
          &&audioOutput.channelCount==2&&audioOutput.busType==Steinberg::Vst::kMain
          &&(audioOutput.flags&Steinberg::Vst::BusInfo::kDefaultActive)!=0,
          "SDK instrument output must be an active main stereo audio bus");
    for (auto direction : {Steinberg::Vst::kInput, Steinberg::Vst::kOutput}) {
        check(component->getBusCount(Steinberg::Vst::kEvent, direction) == 1, "SDK MIDI event bus missing or duplicated");
        Steinberg::Vst::BusInfo info{};
        check(component->getBusInfo(Steinberg::Vst::kEvent, direction, 0, info) == Steinberg::kResultOk,
              "SDK event bus metadata unavailable");
        check(info.mediaType == Steinberg::Vst::kEvent && info.direction == direction
              && info.channelCount == 16 && info.busType == Steinberg::Vst::kMain,
              "SDK event bus must expose all sixteen MIDI channels");
    }
    check(p.hasEditor(), "Actual VST3 editor unavailable");
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorIfNeeded());
        check(editor != nullptr && editor->getWidth() > 0 && editor->getHeight() > 0, "Actual VST3 editor creation failed");
    }
    for (const auto* name : {"Pitch Enabled", "Pitch Mode", "Mono Bend", "MPE Output", "Bend Range",
                            "Input Threshold", "Automatic Input", "Sensitivity", "Release", "Input Dynamics",
                            "Input Gain", "Gate Open Threshold", "Gate Hysteresis", "Gate Attack", "Gate Hold",
                            "Gate Release", "Compressor Threshold", "Compressor Ratio", "Limiter Ceiling"})
        check(findParameter(p, name) == nullptr, std::string("Obsolete audio parameter exposed: ") + name);
    check(parameter(p, "Scale").getNumSteps() == expectedScaleCount, "Actual VST3 must expose 38 scales");
    check(parameter(p, "Mode").getNumSteps() == 3, "Mode indices must remain 0/1/2");
    check(parameter(p, "Mode").getText(0, 128) == "Chord Tones Only"
          && parameter(p, "Mode").getText(.5f, 128) == "Chord-Scale Assignment"
          && parameter(p, "Mode").getText(1, 128) == "Fixed Scale", "Actual VST3 mode labels changed");
    check(parameter(p, "Chord MIDI Through").getValue() == 0, "Chord relay must default off");
    int maps = 0, publicParameters = 0;
    for (auto* item : p.getParameters()) {
        const auto name = item->getName(128);
        // JUCE's VST3 host exposes wrapper bypass and MIDI-controller
        // transport parameters in addition to the product's parameters.
        if (item != p.getBypassParameter() && !name.startsWith("MIDI CC ")) ++publicParameters;
        if (name.startsWith("Map ")) {
            ++maps; check(item->getNumSteps() == expectedScaleCount, "Chord mapping must expose all 38 scales");
        }
    }
    check(maps == 52 && publicParameters == 59, "Actual VST3 must expose seven controls and 52 chord mappings");
    for (const auto* id : chordIds) {
        auto& mapping = parameter(p, "Map " + juce::String(id));
        auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*>(&mapping);
        const auto expectedId = juce::uint32(("map_" + juce::String(id)).hashCode()) & 0x7fffffffu;
        check(hosted != nullptr && hosted->getParameterID() == juce::String(expectedId),
              "Actual VST3 mapping ParamID changed: " + std::string(id));
    }
    expectUserFactoryChoices(p, true);
    juce::MidiBuffer silenceProbe;s.process(silenceProbe);check(silenceProbe.isEmpty(),"Normal silent-output probe generated MIDI unexpectedly");
    s.process(silenceProbe,-1,true);check(silenceProbe.isEmpty(),"Conditional bypass silent-output probe generated MIDI unexpectedly");
    features.add("silent_stereo_audio_output"); features.add("sdk_midi_event_buses"); features.add("instrument_metadata");
    features.add("editor"); features.add("removed_pitch_parameters"); features.add("stable_mapping_parameter_ids");
}
void testUserFactoryState(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session fresh(formats, d, "fresh_user_factory"), restored(formats, d, "restored_user_factory");
    expectUserFactoryChoices(*fresh.plugin, true);
    // Save before any processing or assignment setter on the source instance.
    // Make every destination row wrong first, so defaults cannot hide a bad
    // mixed-factory serialization or an ignored state restoration.
    juce::MemoryBlock state; fresh.plugin->getStateInformation(state);
    check(state.getSize() > 0, "Fresh mixed-factory VST3 state is empty");
    for (size_t i = 0; i < chordIds.size(); ++i)
        setChoice(*restored.plugin, "Map " + juce::String(chordIds[i]), (userFactoryChoices[i] + 1) % expectedScaleCount);
    restored.flushControls();
    restored.plugin->setStateInformation(state.getData(), int(state.getSize())); restored.flushControls();
    expectUserFactoryChoices(*fresh.plugin, false); expectUserFactoryChoices(*restored.plugin, false);
    setChoice(*fresh.plugin, "Mode", 1); setChoice(*restored.plugin, "Mode", 1);
    // Untouched user assignments: Cm13b5 -> Harmonic Major 2 (index32),
    // C13b9 -> Harmonic Major 5 (index35). Both are above the old five-bit
    // status range; these assertions observe the compiled module's MIDI.
    for (int fixture = 0; fixture < 2; ++fixture) {
        juce::MidiBuffer midi;
        if (fixture == 0) chord(midi, true, {48, 50, 51, 53, 54, 57, 58}, 7);
        else chord(midi, true, {48, 49, 52, 53, 55, 57, 58}, 7);
        const int input = fixture == 0 ? 68 : 62, output = fixture == 0 ? 69 : 61;
        midi.addEvent(juce::MidiMessage::noteOn(1, input, juce::uint8(92 + fixture)), 23);
        midi.addEvent(juce::MidiMessage::noteOff(1, input), 47);
        juce::MidiBuffer copied(midi);
        const auto a = fresh.process(midi), b = restored.process(copied);
        check(a.size() == 2 && b.size() == 2, "Fresh/copied user factory chord lost a MIDI lifecycle");
        expectNote(a, 0, true, output, 23, 1, 92 + fixture); expectNote(a, 1, false, output, 47);
        expectNote(b, 0, true, output, 23, 1, 92 + fixture); expectNote(b, 1, false, output, 47);
        midi.clear();
        if (fixture == 0) chord(midi, false, {48, 50, 51, 53, 54, 57, 58}, 55);
        else chord(midi, false, {48, 49, 52, 53, 55, 57, 58}, 55);
        copied = midi; fresh.process(midi); restored.process(copied);
        check(midi.isEmpty() && copied.isEmpty(), "Factory chord controls leaked to the instrument");
    }
    features.add("user_factory_defaults");
}
void testRouting(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d, double rate, int block) {
    Session s(formats, d, "routing_" + juce::String(rate, 0) + "_" + juce::String(block), rate, block);
    juce::MidiBuffer midi;
    // Insert playing input first: controls at the same sample still win.
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(93)), 11); chord(midi, true, {48, 52, 55}, 11);
    midi.addEvent(juce::MidiMessage::noteOff(1, 61, juce::uint8(37)), block - 3);
    auto out = s.process(midi); check(out.size() == 2, "Chord notes leaked or playing input lost");
    expectNote(out, 0, true, 60, 11, 1, 93); expectNote(out, 1, false, 60, block - 3, 1, 37);
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 13, juce::uint8(75)), 1); midi.addEvent(juce::MidiMessage::noteOff(1, 13), 2);
    midi.addEvent(juce::MidiMessage::noteOn(1, 109, juce::uint8(88)), 3); midi.addEvent(juce::MidiMessage::noteOff(1, 109), 4);
    out = s.process(midi); check(out.size() == 4, "Low/high register lifecycle lost");
    expectNote(out, 0, true, 12, 1, 1, 75); expectNote(out, 1, false, 12, 2);
    expectNote(out, 2, true, 108, 3, 1, 88); expectNote(out, 3, false, 108, 4);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(100)), 5);
    chord(midi, false, {48, 52, 55}, block / 2); chord(midi, true, {50, 53, 57}, block / 2);
    midi.addEvent(juce::MidiMessage::noteOn(1, 63, juce::uint8(101)), block / 2);
    midi.addEvent(juce::MidiMessage::noteOff(1, 61), block - 2); midi.addEvent(juce::MidiMessage::noteOff(1, 63), block - 1);
    out = s.process(midi); check(out.size() == 4, "Mid-block chord change lost events");
    expectNote(out, 0, true, 60, 5); expectNote(out, 1, true, 62, block / 2);
    expectNote(out, 2, false, 60, block - 2); expectNote(out, 3, false, 62, block - 1);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 65, juce::uint8(111)), 7);
    midi.addEvent(juce::MidiMessage::noteOn(1, 65, juce::uint8(0)), 19);
    out = s.process(midi); check(out.size() == 2, "Velocity-zero Note On lifecycle lost");
    expectNote(out, 0, true, 65, 7); expectNote(out, 1, false, 65, 19);
}
constexpr unsigned mask(std::initializer_list<int> pcs) {
    unsigned result = 0; for (int pc : pcs) result |= 1u << pc; return result;
}
// Expected formulas in parameter order. Existing indices 0..30 are retained;
// the seven Harmonic Major modes are appended at 31..37.
constexpr std::array<unsigned, expectedScaleCount> scaleMasks{{
    0xfff, mask({0,2,4,5,7,9,11}), mask({0,2,3,5,7,8,10}), mask({0,2,3,5,7,8,11}),
    mask({0,2,3,5,7,9,11}), mask({0,2,3,5,7,9,10}), mask({0,1,3,5,7,8,10}), mask({0,2,4,6,7,9,11}),
    mask({0,2,4,5,7,9,10}), mask({0,1,3,5,6,8,10}), mask({0,2,4,6,8,10}), mask({0,2,4,7,9}),
    mask({0,3,5,7,10}), mask({0,1,3,4,6,7,9,10}), mask({0,2,3,5,6,8,9,11}), mask({0,2,4,6,7,9,10}),
    mask({0,1,3,4,6,8,10}), mask({0,1,4,5,7,8,10}), mask({0,3,5,6,7,10}),
    mask({0,1,3,5,7,9,10}), mask({0,2,4,6,8,9,11}), mask({0,2,4,5,7,8,10}), mask({0,2,3,5,6,8,10}),
    // Harmonic Minor mode 4: subtract its fourth degree (5) from
    // {0,2,3,5,7,8,11}, modulo 12 -> {0,2,3,6,7,9,10} (Dorian #4).
    mask({0,1,3,5,6,9,10}), mask({0,2,4,5,8,9,11}), mask({0,2,3,6,7,9,10}), mask({0,3,4,6,7,9,11}),
    mask({0,1,3,4,6,8,9}), mask({0,2,4,5,7,9,10,11}), mask({0,2,4,5,7,8,9,11}), mask({0,2,3,5,7,9,10,11}),
    mask({0,2,4,5,7,8,11}), mask({0,2,3,5,6,9,10}), mask({0,1,3,4,7,8,10}), mask({0,2,3,6,7,9,11}),
    mask({0,1,4,5,7,9,10}), mask({0,3,4,6,8,9,11}), mask({0,1,3,5,6,8,9})
}};
constexpr bool harmonicMajorModesMatchParent() {
    // Independently derive all seven modes from Harmonic Major (major b6),
    // subtracting each successive parent degree and reducing modulo twelve.
    constexpr int parent[]{0, 2, 4, 5, 7, 8, 11};
    for (int mode = 0; mode < 7; ++mode) {
        unsigned derived = 0;
        for (int pc : parent) derived |= 1u << ((pc - parent[mode] + 12) % 12);
        if (scaleMasks[size_t(31 + mode)] != derived) return false;
    }
    return true;
}
static_assert(harmonicMajorModesMatchParent(), "Host Harmonic Major expectations must match the parent-scale derivation");
void testScales(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session s(formats, d, "all_38_scale_indices"); setChoice(*s.plugin, "Mode", 2);
    for (int scale = 0; scale < expectedScaleCount; ++scale) {
        setChoice(*s.plugin, "Scale", scale); juce::MidiBuffer midi;
        auto& choice = parameter(*s.plugin, "Scale");
        check(std::lround(choice.getValue() * (expectedScaleCount - 1)) == scale,
              "Actual VST3 host did not accept scale index " + std::to_string(scale));
        for (int pc = 0; pc < 12; ++pc) {
            midi.addEvent(juce::MidiMessage::noteOn(1, 60 + pc, juce::uint8(80 + pc)), pc * 4);
            midi.addEvent(juce::MidiMessage::noteOff(1, 60 + pc), pc * 4 + 1);
        }
        const auto out = s.process(midi); check(out.size() == 24, "Scale fixture lost a note lifecycle");
        check(std::lround(choice.getValue() * (expectedScaleCount - 1)) == scale,
              "Actual VST3 host scale changed during processing");
        for (int pc = 0; pc < 12; ++pc) {
            const auto& on = out[size_t(pc * 2)];
            check(on.on && on.channel == 1 && on.offset == pc * 4 && on.velocity == 80 + pc,
                  "Scale changed channel, velocity or sample offset");
            check((scaleMasks[size_t(scale)] & (1u << (on.pitch % 12))) != 0,
                  "Compiled scale index " + std::to_string(scale) + " ("
                  + choice.getText(choice.getValue(), 128).toStdString() + ") emitted "
                  + std::to_string(on.pitch) + " for input " + std::to_string(60 + pc)
                  + " outside the expected set");
            if (scaleMasks[size_t(scale)] & (1u << pc))
                check(on.pitch == 60 + pc, "Allowed note moved at retained scale index " + std::to_string(scale));
            int distance = 12;
            for (int delta = -12; delta <= 12; ++delta)
                if (scaleMasks[size_t(scale)] & (1u << ((pc + delta + 12) % 12))) distance = std::min(distance, std::abs(delta));
            check(std::abs(on.pitch - (60 + pc)) == distance, "Compiled scale did not choose a nearest pitch");
            expectNote(out, size_t(pc * 2 + 1), false, on.pitch, pc * 4 + 1);
        }
    }
    features.add("all_38_scales"); features.add("legacy_31_scale_indices");
    features.add("legacy_19_scale_indices"); features.add("harmonic_major_modes");
}
void testInternalCorrection(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session s(formats, d, "internal_correction_off_held_chord"); setNormalised(*s.plugin, "Note Correction", 0);
    juce::MidiBuffer midi; midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(91)), 11);
    chord(midi, true, {48, 52, 55}, 11); auto out = s.process(midi);
    check(out.size() == 1, "Internal correction off must consume controls and pass playing input");
    expectNote(out, 0, true, 61, 11, 1, 91);
    setNormalised(*s.plugin, "Note Correction", 1); midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 63, juce::uint8(89)), 9);
    midi.addEvent(juce::MidiMessage::noteOff(1, 61), 21); midi.addEvent(juce::MidiMessage::noteOff(1, 63), 27);
    out = s.process(midi); check(out.size() == 3, "Correction on lost held controls or old bindings");
    expectNote(out, 0, true, 64, 9, 1, 89); expectNote(out, 1, false, 61, 21); expectNote(out, 2, false, 64, 27);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 66, juce::uint8(90)), 4);
    out = s.process(midi); check(out.size() == 1, "Held binding fixture failed"); expectNote(out, 0, true, 67, 4);
    setNormalised(*s.plugin, "Note Correction", 0); midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 66), 7);
    midi.addEvent(juce::MidiMessage::noteOn(1, 66, juce::uint8(90)), 13); midi.addEvent(juce::MidiMessage::noteOff(1, 66), 19);
    out = s.process(midi); check(out.size() == 3, "Correction change cut an existing binding");
    expectNote(out, 0, false, 67, 7); expectNote(out, 1, true, 66, 13); expectNote(out, 2, false, 66, 19);
    features.add("internal_correction_bypass"); features.add("held_note_off_bindings");
}
void testNewChordMappings(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    for (int type = 50; type < 52; ++type) {
        Session s(formats, d, "new_chord_" + juce::String(chordIds[size_t(type)]));
        // Every other assignment is chromatic, so selecting the wrong
        // recognized type cannot accidentally produce the expected correction.
        for (const auto* id : chordIds) setChoice(*s.plugin, "Map " + juce::String(id), 0);
        setChoice(*s.plugin, "Map " + juce::String(chordIds[size_t(type)]), type == 50 ? 10 : 11);
        juce::MidiBuffer midi;
        if (type == 50) chord(midi, true, {48, 50, 52, 54, 55, 57, 59}, 7);
        else chord(midi, true, {48, 50, 52, 54, 55, 59}, 7);
        midi.addEvent(juce::MidiMessage::noteOn(1, 65, juce::uint8(90)), 23);
        midi.addEvent(juce::MidiMessage::noteOff(1, 65), 41);
        auto out = s.process(midi); check(out.size() == 2, "New chord-tone lifecycle missing");
        expectNote(out, 0, true, 66, 23); expectNote(out, 1, false, 66, 41);
        setChoice(*s.plugin, "Mode", 1);
        const int input = type == 50 ? 69 : 66, output = type == 50 ? 70 : 67;
        midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, input, juce::uint8(91)), 29);
        midi.addEvent(juce::MidiMessage::noteOff(1, input), 43);
        out = s.process(midi); check(out.size() == 2, "New independent assignment lifecycle missing");
        expectNote(out, 0, true, output, 29, 1, 91); expectNote(out, 1, false, output, 43);
    }
    features.add("new_52_chord_mappings");
}
void testSerial(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session first(formats, d, "serial_first"), middle(formats, d, "serial_middle_off"), last(formats, d, "serial_last");
    setNormalised(*first.plugin, "Chord MIDI Through", 1); setNormalised(*middle.plugin, "Chord MIDI Through", 1);
    setNormalised(*middle.plugin, "Note Correction", 0); setNormalised(*last.plugin, "Note Correction", 0);
    first.flushControls(); middle.flushControls(); last.flushControls();
    juce::MidiBuffer midi; chord(midi, true, {48, 52, 55}, 3);
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(92)), 9); midi.addEvent(juce::MidiMessage::noteOff(1, 61), 17);
    auto out = first.process(midi); check(out.size() == 5, "First serial instance failed chord relay");
    expectNote(out, 0, true, 48, 3, 16); expectNote(out, 1, true, 52, 3, 16); expectNote(out, 2, true, 55, 3, 16);
    expectNote(out, 3, true, 60, 9, 1, 92); expectNote(out, 4, false, 60, 17);
    const juce::MidiBuffer firstOutput(midi); middle.process(midi);
    check(midi.getNumEvents() == firstOutput.getNumEvents(), "Internal-off middle instance lost relay events");
    auto a = midi.begin(), b = firstOutput.begin();
    for (; a != midi.end(); ++a, ++b) {
        const auto x = (*a).getMessage(), y = (*b).getMessage();
        check((*a).samplePosition == (*b).samplePosition && x.getRawDataSize() == y.getRawDataSize()
              && std::equal(x.getRawData(), x.getRawData() + x.getRawDataSize(), y.getRawData()),
              "Internal-off middle instance changed serial bytes or offset");
    }
    out = last.process(midi); check(out.size() == 2, "Final instance must consume chord controls");
    expectNote(out, 0, true, 60, 9, 1, 92); expectNote(out, 1, false, 60, 17);
    // Last learned the held chord while off; first now corrects to D Ionian.
    setChoice(*first.plugin, "Mode", 2); setChoice(*first.plugin, "Root", 2); setChoice(*first.plugin, "Scale", 1);
    setNormalised(*last.plugin, "Note Correction", 1); midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(95)), 23); midi.addEvent(juce::MidiMessage::noteOff(1, 60), 45);
    out = first.process(midi); check(out.size() == 2, "First cascade lifecycle missing");
    expectNote(out, 0, true, 61, 23); expectNote(out, 1, false, 61, 45);
    middle.process(midi); out = last.process(midi); check(out.size() == 2, "Second active correction lost cascade lifecycle");
    expectNote(out, 0, true, 60, 23, 1, 95); expectNote(out, 1, false, 60, 45);
    // Here the cascade differs from the last stage alone: C chord tones
    // would send input F (65) to E (64), but D Ionian first sends it to F#.
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 65, juce::uint8(96)), 23);
    midi.addEvent(juce::MidiMessage::noteOff(1, 65), 45);
    out = first.process(midi); check(out.size() == 2, "Distinct cascade first lifecycle missing");
    expectNote(out, 0, true, 66, 23, 1, 96); expectNote(out, 1, false, 66, 45);
    middle.process(midi); out = last.process(midi); check(out.size() == 2, "Distinct cascade final lifecycle missing");
    expectNote(out, 0, true, 67, 23, 1, 96); expectNote(out, 1, false, 67, 45);
    midi.clear(); chord(midi, false, {48, 52, 55}, 63); first.process(midi); middle.process(midi); last.process(midi);
    check(midi.isEmpty(), "Final stage leaked chord releases");
    features.add("chord_channel_16"); features.add("chord_relay"); features.add("serial_correction");
}
void testExpression(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session s(formats, d, "non_ch1_notes_and_expression");
    for (int channel : {2, 3, 15}) {
        juce::MidiBuffer midi; midi.addEvent(juce::MidiMessage::noteOn(channel, 61, juce::uint8(87)), 11);
        midi.addEvent(juce::MidiMessage::aftertouchChange(channel, 61, 73), 19);
        midi.addEvent(juce::MidiMessage::noteOff(channel, 61, juce::uint8(42)), 31);
        const auto out = s.process(midi); check(midi.getNumEvents() == 3 && out.size() == 2, "Raw note/expression lifecycle missing");
        expectNote(out, 0, true, 61, 11, channel, 87); expectNote(out, 1, false, 61, 31, channel, 42);
        auto it = midi.begin(); ++it; const auto m = (*it).getMessage();
        check(m.isAftertouch() && m.getChannel() == channel && m.getNoteNumber() == 61
              && m.getAfterTouchValue() == 73 && (*it).samplePosition == 19, "Non-channel-one polyphonic expression changed");
    }
    for (const auto& expected : std::vector<juce::MidiMessage>{juce::MidiMessage::controllerEvent(2, 1, 73),
             juce::MidiMessage::pitchWheel(2, 12345), juce::MidiMessage::channelPressureChange(2, 41),
             juce::MidiMessage::programChange(2, 17)}) {
        juce::MidiBuffer midi; midi.addEvent(expected, 17); s.process(midi);
        check(midi.getNumEvents() == 1, "Non-note MIDI event missing or duplicated");
        const auto actual = (*midi.begin()).getMessage();
        check(actual.getRawDataSize() == expected.getRawDataSize()
              && std::equal(actual.getRawData(), actual.getRawData() + actual.getRawDataSize(), expected.getRawData()),
              "Actual VST3 changed non-note expression bytes");
        check((*midi.begin()).samplePosition == 17, "Actual VST3 changed expression sample offset");
    }
    features.add("non_ch1_passthrough"); features.add("expression_offsets");
}
void testPedals(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session s(formats, d, "channel1_sustain_and_latch_reset"); juce::MidiBuffer midi; chord(midi, true, {48, 52, 55}, 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 1);
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(90)), 11); midi.addEvent(juce::MidiMessage::noteOff(1, 61), 21);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 31);
    auto out = s.process(midi); check(out.size() == 2 && midi.getNumEvents() == 2, "Core sustain lifecycle or CC64 consumption changed");
    expectNote(out, 0, true, 60, 11); expectNote(out, 1, false, 60, 31);
    midi.clear(); midi.addEvent(juce::MidiMessage::controllerEvent(1, 66, 127), 3);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 69, 127), 7); s.process(midi);
    check(midi.getNumEvents() == 2, "Destination latch controllers missing");
    s.timeline.playing = false; midi.clear(); out = s.process(midi);
    check(out.empty() && midi.getNumEvents() == 2, "Stop failed to reset remembered latch pedals");
    bool up66 = false, up69 = false;
    for (const auto metadata : midi) {
        const auto m = metadata.getMessage();
        check(m.isController() && m.getChannel() == 1 && m.getControllerValue() == 0 && metadata.samplePosition == 0,
              "Latch reset must be a channel-one controller release at sample zero");
        if (m.getControllerNumber() == 66) up66 = true; if (m.getControllerNumber() == 69) up69 = true;
    }
    check(up66 && up69, "Stop failed to release both latch pedals");
    midi.clear(); s.process(midi); check(midi.isEmpty(), "Stop repeated latch releases"); features.add("channel1_pedals");
}
void testOverlappingBindings(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session s(formats, d, "overlapping_input_bindings"); juce::MidiBuffer midi; chord(midi, true, {48, 52, 55}, 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(81)), 3);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, juce::uint8(95)), 7);
    midi.addEvent(juce::MidiMessage::noteOff(1, 61), 11); midi.addEvent(juce::MidiMessage::noteOff(1, 60), 19);
    auto out = s.process(midi); check(out.size() == 2, "Shared corrected output lost reference ownership");
    expectNote(out, 0, true, 60, 3, 1, 81); expectNote(out, 1, false, 60, 19);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(82)), 5);
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(92)), 9);
    midi.addEvent(juce::MidiMessage::noteOff(1, 61), 17); midi.addEvent(juce::MidiMessage::noteOff(1, 61), 29);
    out = s.process(midi); check(out.size() == 2, "Repeated input FIFO lost corrected output ownership");
    expectNote(out, 0, true, 60, 5, 1, 82); expectNote(out, 1, false, 60, 29);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1, 61), 31); out = s.process(midi);
    check(out.empty(), "Unmatched note release generated a spurious output");
    features.add("overlapping_note_bindings");
}
void testState(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session original(formats, d, "state_original"), restored(formats, d, "state_clone"), independent(formats, d, "state_independent");
    struct Saved { juce::String name; float value, independent; }; std::vector<Saved> expected;
    int index = 0, maps = 0;
    for (auto* item : original.plugin->getParameters()) {
        const auto name = item->getName(128);
        if (name != "Note Correction" && name != "Mode" && name != "Root" && name != "Scale"
            && name != "Chord MIDI Through" && name != "Context Help" && !name.startsWith("Map ")) continue;
        const int steps = item->getNumSteps(); check(steps >= 2 && steps <= expectedScaleCount, "Unexpected saved MIDI parameter range");
        const float varied = float((index++ * 7 + 1) % steps) / float(steps - 1);
        // The fixed-scale state must also carry the newly appended last mode.
        const float value = name == "Scale" ? 1.0f : varied; item->setValueNotifyingHost(value);
        expected.push_back({name, value, parameter(*independent.plugin, name).getValue()}); if (name.startsWith("Map ")) ++maps;
    }
    check(expected.size() == 58 && maps == 52, "State must cover six MIDI/UI controls and 52 assignments");
    original.flushControls(); juce::MemoryBlock state; original.plugin->getStateInformation(state);
    check(state.getSize() > 0, "Actual VST3 state empty");
    restored.plugin->setStateInformation(state.getData(), int(state.getSize())); restored.flushControls();
    for (const auto& saved : expected) {
        check(std::abs(parameter(*restored.plugin, saved.name).getValue() - saved.value) < .00001f,
              "Actual VST3 state roundtrip failed: " + saved.name.toStdString());
        check(parameter(*independent.plugin, saved.name).getValue() == saved.independent, "One instance changed another's settings");
    }
    const auto map = std::find_if(expected.begin(), expected.end(), [](const Saved& s) { return s.name == "Map major"; });
    check(map != expected.end(), "Major chord mapping missing");
    setChoice(*original.plugin, "Map major", map->value == 0 ? expectedScaleCount - 1 : 0);
    check(std::abs(parameter(*restored.plugin, "Map major").getValue() - map->value) < .00001f, "Cloned assignment changed with source instance");
    // Also exercise a newly appended scale through the actual assignment
    // path. A copied C-major assignment remains Harmonic Major mode 7 after
    // the source instance switches that assignment to Ionian.
    setChoice(*original.plugin, "Mode", 1); setNormalised(*original.plugin, "Note Correction", 1);
    setNormalised(*original.plugin, "Chord MIDI Through", 0); setChoice(*original.plugin, "Map major", 37);
    original.flushControls(); original.plugin->getStateInformation(state);
    restored.plugin->setStateInformation(state.getData(), int(state.getSize())); restored.flushControls();
    setChoice(*original.plugin, "Map major", 1);
    juce::MidiBuffer assignment; chord(assignment, true, {48, 52, 55}, 0);
    assignment.addEvent(juce::MidiMessage::noteOn(1, 63, juce::uint8(94)), 11);
    assignment.addEvent(juce::MidiMessage::noteOff(1, 63), 27);
    juce::MidiBuffer clonedAssignment(assignment);
    const auto sourceAssignment = original.process(assignment), cloneAssignment = restored.process(clonedAssignment);
    check(sourceAssignment.size() == 2 && cloneAssignment.size() == 2, "Instance assignment path lost a note lifecycle");
    expectNote(sourceAssignment, 0, true, 64, 11); expectNote(sourceAssignment, 1, false, 64, 27);
    expectNote(cloneAssignment, 0, true, 63, 11); expectNote(cloneAssignment, 1, false, 63, 27);
    assignment.clear(); chord(assignment, false, {48, 52, 55}, 0); clonedAssignment = assignment;
    original.process(assignment); restored.process(clonedAssignment);
    // State contains settings, never the held keys or active note mappings.
    setChoice(*original.plugin, "Mode", 0); setNormalised(*original.plugin, "Note Correction", 1);
    setNormalised(*original.plugin, "Chord MIDI Through", 0); original.flushControls();
    juce::MidiBuffer midi; chord(midi, true, {48, 52, 55}, 0); midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(90)), 11);
    const auto out = original.process(midi); check(out.size() == 1, "State-held-note fixture failed"); expectNote(out, 0, true, 60, 11);
    original.plugin->getStateInformation(state); restored.plugin->setStateInformation(state.getData(), int(state.getSize()));
    midi.clear(); auto cloneOut = restored.process(midi); check(cloneOut.empty(), "State revived a held MIDI note");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1, 61), 7); cloneOut = restored.process(midi);
    check(cloneOut.empty(), "State revived an active voice binding");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(90)), 11); cloneOut = restored.process(midi);
    check(cloneOut.empty(), "State revived held chord controls"); features.add("instance_state");
}
void testTransport(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session s(formats, d, "transport_reset"); juce::MidiBuffer midi; chord(midi, true, {48, 52, 55}, 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(80)), 17); auto out = s.process(midi);
    check(out.size() == 1, "Transport onset fixture failed");
    s.timeline.playing = false; midi.clear(); out = s.process(midi); check(out.size() == 1, "Stop failed to release held note");
    expectNote(out, 0, false, 60, 0);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(80)), 1); out = s.process(midi);
    check(out.empty(), "Stop retained stale chord controls");
    s.timeline.playing = true; midi.clear(); chord(midi, true, {48, 52, 55}, 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(80)), 17); out = s.process(midi);
    check(out.size() == 1, "Restart onset fixture failed");
    s.timeline.sample = 0; midi.clear(); out = s.process(midi); check(out.size() == 1, "Seek failed to release held note");
    expectNote(out, 0, false, 60, 0); midi.clear(); s.process(midi); check(midi.isEmpty(), "Seek repeated releases");
    features.add("transport_reset");
}
void testGeneratedRouting(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session s(formats, d, "generated_midi_routing"), restored(formats, d, "generated_midi_state_clone");
    setNormalised(*s.plugin, "Note Correction", 0); s.flushControls();
    setNormalised(*s.plugin, "Routing Test", 1);
    std::vector<Note> generated; int external = 0;
    for (int start = 0; start < 5000; start += 128) {
        juce::MidiBuffer midi;
        if (start == 0) {
            midi.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(77)), 11);
            midi.addEvent(juce::MidiMessage::noteOff(1, 64), 31);
        }
        for (auto n : s.process(midi)) {
            if (n.pitch == 60) { n.offset += start; generated.push_back(n); }
            else {
                check(n.pitch == 64, "Unexpected diagnostic MIDI pitch");
                if (n.on) check(n.offset == 11 && n.velocity == 77, "Diagnostic MIDI changed external onset");
                else check(n.offset == 31, "Diagnostic MIDI changed external release");
                ++external;
            }
        }
    }
    check(external == 2 && generated.size() == 2, "Diagnostic routing must coexist with external MIDI and emit one lifecycle");
    expectNote(generated, 0, true, 60, 0, 1, 100); expectNote(generated, 1, false, 60, 4800);
    setNormalised(*s.plugin, "Routing Test", 0); s.flushControls();
    setNormalised(*s.plugin, "Routing Test", 1); juce::MidiBuffer midi; auto out = s.process(midi);
    check(out.size() == 1, "Diagnostic MIDI did not re-arm"); expectNote(out, 0, true, 60, 0);
    juce::MemoryBlock state; s.plugin->getStateInformation(state);
    restored.plugin->setStateInformation(state.getData(), int(state.getSize())); midi.clear();
    auto cloneOut = restored.process(midi);
    check(cloneOut.empty() && parameter(*restored.plugin, "Routing Test").getValue() == 0,
          "State must never re-arm the transient MIDI routing test");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 64, juce::uint8(72)), 5); out = s.process(midi);
    check(out.size() == 1, "External sustained-note fixture failed"); expectNote(out, 0, true, 64, 5);
    setNormalised(*s.plugin, "Routing Test", 0); midi.clear(); out = s.process(midi);
    check(out.size() == 1, "Stopping diagnostic MIDI must release only that source"); expectNote(out, 0, false, 60, 0);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1, 64), 17); out = s.process(midi);
    check(out.size() == 1, "Diagnostic stop lost external binding"); expectNote(out, 0, false, 64, 17);
    features.add("generated_midi_routing");
}
void testBypassCallback(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& d) {
    Session s(formats, d, "host_invoked_bypass_callback"); juce::MidiBuffer midi; chord(midi, true, {48, 52, 55}, 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(82)), 11); midi.addEvent(juce::MidiMessage::noteOff(1, 61), 31);
    auto out = s.process(midi, -1, true); check(out.size() == 2, "Invoked bypass callback lost MIDI");
    expectNote(out, 0, true, 61, 11); expectNote(out, 1, false, 61, 31);
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 61, juce::uint8(82)), 13); midi.addEvent(juce::MidiMessage::noteOff(1, 61), 29);
    out = s.process(midi); check(out.size() == 2, "Normal callback lost controls learned during invoked bypass");
    expectNote(out, 0, true, 60, 13); expectNote(out, 1, false, 60, 29);
    features.add("host_invoked_bypass_callback_only");
}
void writeEvidence(const juce::File& directory, bool passed, const juce::String& error) {
    check(directory.createDirectory().wasOk(), "Could not create VST3 evidence directory");
    const auto events = directory.getChildFile("Flysnapper-midi-vst3-events.csv");
    check(events.replaceWithText(csv), "Could not write VST3 MIDI events");
    auto* object = new juce::DynamicObject;
    object->setProperty("schema", 1); object->setProperty("kind", "FlysnapperMidiOnlyVst3");
    object->setProperty("version", "0.4.1");
    object->setProperty("status", passed ? "passed" : "failed"); object->setProperty("checks", assertions);
    object->setProperty("scope", "Compiled instrument VST3 executed by the JUCE host with actual stereo output buffers seeded nonzero and verified silent during normal and conditional bypass processing; no DAW or audio driver");
    object->setProperty("ableton_verified", false); object->setProperty("pitch_detection_implemented", false);
    object->setProperty("audio_input_channels", 0); object->setProperty("audio_output_channels", 2);
    object->setProperty("audio_input_buses", 0); object->setProperty("audio_output_buses", 1);
    object->setProperty("host_device_role","instrument");
    object->setProperty("audio_output_silent",passed&&silentOutputSamplesChecked>0&&bypassSilenceCallbacks>0);
    object->setProperty("silent_output_samples_checked",juce::int64(silentOutputSamplesChecked));
    object->setProperty("bypass_silence_callbacks",int(bypassSilenceCallbacks));
    object->setProperty("declared_latency_samples", 0); object->setProperty("scale_count", expectedScaleCount); object->setProperty("chord_type_count", 52);
    object->setProperty("public_parameter_count", 59);
    object->setProperty("factory_mapping_sha256", factoryMappingSha256);
    object->setProperty("cases", juce::var(cases)); object->setProperty("verified_features", juce::var(features));
    object->setProperty("events_file", events.getFileName()); object->setProperty("events_rows", eventRows);
    object->setProperty("limitations", "No Ableton Live12.4.6 execution or loading/placement claim. This is an instrument-role plugin, not a native MIDI effect. Serial tests explicitly connect instances in this driver, not a same-track Live chain or internal rack. Bypass callbacks are invoked explicitly; no Device Activator scheduling, direct-endpoint fan-out, held-chord replay or downstream-instrument claim.");
    if (error.isNotEmpty()) object->setProperty("error", error);
    check(directory.getChildFile("Flysnapper-midi-vst3-evaluation.json").replaceWithText(juce::JSON::toString(juce::var(object), false)),
          "Could not write VST3 evaluation");
}
int run(int argc, const juce::String& path, const juce::String& output) {
    const auto directory = output.isNotEmpty() ? juce::File(output) : juce::File::getCurrentWorkingDirectory();
    try {
        juce::ScopedJuceInitialiser_GUI gui;
        check(argc == 2 || argc == 3, "Expected VST3 bundle path and optional evidence directory");
        const juce::File bundle(path); check(bundle.isDirectory(), "Actual VST3 bundle not found");
        juce::AudioPluginFormatManager formats; auto vst = std::make_unique<juce::VST3PluginFormat>();
        auto* format = vst.get(); formats.addFormat(std::move(vst)); juce::OwnedArray<juce::PluginDescription> descriptions;
        format->findAllTypesForFile(descriptions, bundle.getFullPathName());
        check(descriptions.size() == 1 && descriptions[0]->name == "Flysnapper MIDI", "Actual discovery returned the wrong class");
        const auto& d = *descriptions[0]; runCase("module_contract", [&] { testModule(formats, d); });
        runCase("user_factory_defaults_and_fresh_state", [&] { testUserFactoryState(formats, d); });
        for (double rate : {44100., 48000., 96000.}) for (int block : {32, 128, 257})
            runCase("routing_" + juce::String(rate, 0) + "_" + juce::String(block), [&] { testRouting(formats, d, rate, block); });
        features.add("note_sample_offsets");
        runCase("all_38_scale_indices", [&] { testScales(formats, d); });
        runCase("internal_correction_off_held_chord", [&] { testInternalCorrection(formats, d); });
        runCase("new_sharp_eleventh_chord_mappings", [&] { testNewChordMappings(formats, d); });
        runCase("serial_instances_and_two_active_corrections", [&] { testSerial(formats, d); });
        runCase("non_ch1_notes_and_expression", [&] { testExpression(formats, d); });
        runCase("channel1_sustain_and_latch_reset", [&] { testPedals(formats, d); });
        runCase("overlapping_input_bindings", [&] { testOverlappingBindings(formats, d); });
        runCase("instance_state", [&] { testState(formats, d); });
        runCase("transport_reset", [&] { testTransport(formats, d); });
        runCase("generated_midi_routing", [&] { testGeneratedRouting(formats, d); });
        runCase("host_invoked_bypass_callback", [&] { testBypassCallback(formats, d); });
        writeEvidence(directory, true, {});
        std::cout << "RESULT actual VST3 host tests passed (" << assertions << " assertions).\n";
#if JUCE_WINDOWS
        std::cout << "PLATFORM VERIFIED: Windows x64 compiled VST3 loaded and executed by native test host.\n";
#else
        std::cout << "PLATFORM VERIFIED: native Linux VST3; Windows is a separate result.\n";
#endif
        std::cout << "NOT VERIFIED: Ableton Live 12.4.6, Device Activator scheduling, direct routing or held-note replay.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n'; try { writeEvidence(directory, false, e.what()); } catch (...) {} return 1;
    }
}
} // namespace
#if JUCE_WINDOWS
int wmain(int argc, wchar_t** argv) {
    return run(argc, argc >= 2 ? juce::String(argv[1]) : juce::String(), argc >= 3 ? juce::String(argv[2]) : juce::String());
}
#else
int main(int argc, char** argv) {
    return run(argc, argc >= 2 ? juce::String::fromUTF8(argv[1]) : juce::String(), argc >= 3 ? juce::String::fromUTF8(argv[2]) : juce::String());
}
#endif
