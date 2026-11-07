// SPDX-License-Identifier: AGPL-3.0-or-later
// Real processor callbacks and explicitly connected processor chains.
// This executable is not Ableton Live and does not verify Live's placement,
// channel routing, Device Activator, direct-endpoint delivery or note chase.
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
thread_local bool watchAllocations = false;
thread_local unsigned allocations = 0;
void* allocate(std::size_t size) {
    if (watchAllocations) ++allocations;
    if (auto* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
}
void* operator new(std::size_t n) { return allocate(n); }
void* operator new[](std::size_t n) { return allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
using Event = std::pair<int, juce::MidiMessage>;
using Stream = std::vector<Event>;
unsigned checks = 0;
juce::Array<juce::var> cases;
juce::StringArray verifiedFeatures;
juce::StringArray eventCases;
std::ostringstream csv;
int64_t eventRows = 0;

void check(bool value, const std::string& message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
void feature(const char* name) {
    if (!verifiedFeatures.contains(name)) verifiedFeatures.add(name);
}
void passed(const char* name, const char* scope) {
    auto* row = new juce::DynamicObject;
    row->setProperty("name", name);
    row->setProperty("status", "passed");
    row->setProperty("scope", scope);
    cases.add(juce::var(row));
}
void set(FlysnapperProcessor& p, const juce::String& id, float value) {
    auto* parameter = p.parameters.getParameter(id);
    check(parameter != nullptr, "Missing MIDI parameter: " + id.toStdString());
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(FlysnapperProcessor& p, const juce::String& id) {
    auto* value = p.parameters.getRawParameterValue(id);
    check(value != nullptr, "Missing raw MIDI parameter: " + id.toStdString());
    return value->load();
}
juce::MidiMessage on(int note, int channel = 1, int velocity = 100) {
    return juce::MidiMessage::noteOn(channel, note, juce::uint8(velocity));
}
juce::MidiMessage off(int note, int channel = 1) {
    return juce::MidiMessage::noteOff(channel, note);
}
Stream chord(std::initializer_list<int> notes, bool noteOn = true, int offset = 0) {
    Stream result;
    for (int note : notes) result.emplace_back(offset, noteOn ? on(note, 16) : off(note, 16));
    return result;
}
void append(Stream& into, const Stream& from) { into.insert(into.end(), from.begin(), from.end()); }
bool same(const Event& a, const Event& b) {
    return a.first == b.first && a.second.getRawDataSize() == b.second.getRawDataSize()
        && std::equal(a.second.getRawData(), a.second.getRawData() + a.second.getRawDataSize(), b.second.getRawData());
}
void sameStream(const Stream& a, const Stream& b, const char* message) {
    check(a.size() == b.size(), std::string(message) + ": event count differs");
    for (size_t i = 0; i < a.size(); ++i) check(same(a[i], b[i]), std::string(message) + ": bytes/offset differ");
}
Stream channel(const Stream& stream, int ch) {
    Stream result;
    for (const auto& event : stream) if (event.second.isForChannel(ch)) result.push_back(event);
    return result;
}
Stream notes(const Stream& stream, bool noteOn) {
    Stream result;
    for (const auto& event : stream)
        if (noteOn ? event.second.isNoteOn() : event.second.isNoteOff()) result.push_back(event);
    return result;
}
void expectNote(const Stream& stream, bool noteOn, int note, int offset, int ch = 1, int velocity = -1) {
    check(stream.size() == 1, "Expected exactly one MIDI event");
    const auto& event = stream.front();
    check(event.first == offset, "MIDI decision offset changed");
    check(noteOn ? event.second.isNoteOn() : event.second.isNoteOff(), "Wrong note event kind");
    check(event.second.getChannel() == ch && event.second.getNoteNumber() == note, "Wrong note/channel");
    if (velocity >= 0) check(event.second.getVelocity() == velocity, "Note velocity changed");
}

struct Harness {
    std::unique_ptr<FlysnapperProcessor> processor = std::make_unique<FlysnapperProcessor>();
    std::string name;
    int64_t position = 0;
    std::array<std::array<int, 128>, 16> active{};
    explicit Harness(std::string caseName) : name(std::move(caseName)) {
        processor->prepareToPlay(48000, 128);
        process(64); // Complete the ordinary fresh-prepare reset before assertions.
    }
    Stream process(int samples, const Stream& incoming = {}, bool bypassCallback = false) {
        juce::AudioBuffer<float> audio(0, samples);
        juce::MidiBuffer midi;
        midi.ensureSize(262144);
        for (const auto& event : incoming) midi.addEvent(event.second, event.first);
        if (bypassCallback) processor->processBlockBypassed(audio, midi);
        else processor->processBlock(audio, midi);
        check(audio.getNumChannels() == 0 && audio.getNumSamples() == samples, "MIDI callback changed the zero-audio buffer");
        // Report the raw driver/instance identities separately from the
        // logical assertion groups. Record only names with actual CSV rows.
        if (!midi.isEmpty()) eventCases.addIfNotAlreadyThere(juce::String::fromUTF8(name.c_str()));
        Stream result;
        for (const auto meta : midi) {
            check(meta.samplePosition >= 0 && meta.samplePosition < std::max(1, samples), "Non-causal MIDI offset");
            const auto m = meta.getMessage();
            if (m.isNoteOnOrOff()) {
                const int ch = m.getChannel() - 1, note = m.getNoteNumber();
                if (m.isNoteOn()) ++active[size_t(ch)][size_t(note)];
                else {
                    check(active[size_t(ch)][size_t(note)] > 0, "Unmatched processor output Note Off");
                    --active[size_t(ch)][size_t(note)];
                }
            }
            if (m.isController() && (m.getControllerNumber() == 120 || m.getControllerNumber() == 123))
                active[size_t(m.getChannel() - 1)].fill(0);
            const char type = m.isNoteOn() ? 'N' : m.isNoteOff() ? 'F' : m.isController() ? 'C'
                : m.isPitchWheel() ? 'P' : m.isChannelPressure() || m.isAftertouch() ? 'A' : m.isSysEx() ? 'S' : 'O';
            const int number = m.isNoteOnOrOff() || m.isAftertouch() ? m.getNoteNumber() : m.isController() ? m.getControllerNumber() : 0;
            const int value = m.isNoteOnOrOff() ? m.getVelocity() : m.isController() ? m.getControllerValue()
                : m.isPitchWheel() ? m.getPitchWheelValue() : m.isChannelPressure() ? m.getChannelPressureValue()
                : m.isAftertouch() ? m.getAfterTouchValue() : 0;
            csv << name << ",48000," << samples << ',' << position + meta.samplePosition << ',' << meta.samplePosition << ',' << type << ','
                << ((m.getRawData()[0] & 0xf0) == 0xf0 ? 0 : m.getChannel()) << ',' << number << ',' << value << ','
                << juce::String::toHexString(m.getRawData(), m.getRawDataSize()).toStdString() << '\n';
            ++eventRows;
            result.emplace_back(meta.samplePosition, m);
        }
        position += samples;
        return result;
    }
    void released() const {
        for (const auto& ch : active) for (int count : ch) check(count == 0, "A processor test leaves output notes active");
    }
};

void declarationsAndState() {
    Harness h("parameters_and_instance_state");
    auto& a = *h.processor;
    FlysnapperProcessor b;
    check(a.acceptsMidi() && a.producesMidi() && a.isMidiEffect(), "Processor does not declare a MIDI effect");
    check(a.getBusCount(true) == 0 && a.getBusCount(false) == 0, "An audio bus remains");
    check(a.getTotalNumInputChannels() == 0 && a.getTotalNumOutputChannels() == 0, "Audio channels remain");
    check(a.getLatencySamples() == 0 && a.getTailLengthSeconds() == 0, "MIDI processor adds a declared delay or tail");
    juce::AudioProcessor::BusesLayout nonMidi;
    nonMidi.inputBuses.add(juce::AudioChannelSet::stereo());
    nonMidi.outputBuses.add(juce::AudioChannelSet::stereo());
    check(!a.isBusesLayoutSupported(nonMidi), "A nonzero audio layout was accepted");
    for (const char* id : {"pitchEnabled", "pitchMode", "monoBend", "bendRange", "mpeOutput", "autoInput", "inputGate",
                           "sensitivity", "releaseMs", "dynamicsEnabled", "inputGainDb", "gateThresholdDb", "gateHysteresisDb",
                           "gateAttackMs", "gateHoldMs", "gateReleaseMs", "compressorThresholdDb", "compressorRatio", "limiterCeilingDb"})
        check(a.parameters.getParameter(id) == nullptr, std::string("Removed audio parameter remains: ") + id);
    check(get(a, "chordThru") == 0, "A fresh last/single instance leaks chord controls");
    auto* mode = dynamic_cast<juce::AudioParameterChoice*>(a.parameters.getParameter("mode"));
    check(mode && mode->choices[0] == "Chord Tones Only" && mode->choices[1] == "Chord-Scale Assignment", "New mode terminology missing");
    check(flysnapper::scaleCount == 31 && flysnapper::chordTypeCount == 50, "Wrong MIDI-only data dimensions");
    for (int i = 0; i < 50; ++i) {
        auto* p = dynamic_cast<juce::AudioParameterChoice*>(a.parameters.getParameter(FlysnapperProcessor::mappingParameterId(i)));
        check(p && p->choices.size() == 31, "A mapping does not offer all31 scales");
        const int expected = flysnapper::factoryDefaults()[size_t(i)];
        check(p->getIndex() == expected && get(a, FlysnapperProcessor::mappingParameterId(i)) == float(expected),
              "Initial raw factory choice differs from its discrete parameter index");
    }
    feature("zero_audio_buses");
    for (int s = 0; s < 31; ++s) {
        for (int i = 0; i < 50; ++i) set(a, FlysnapperProcessor::mappingParameterId(i), float((i + s) % 31));
        set(a, "mode", 2); set(a, "scale", float(s)); set(a, "root", float(s % 12)); set(a, "chordThru", float(s % 2));
        juce::MemoryBlock state;
        a.getStateInformation(state);
        b.setStateInformation(state.getData(), int(state.getSize()));
        for (int i = 0; i < 50; ++i)
            check(get(b, FlysnapperProcessor::mappingParameterId(i)) == float((i + s) % 31), "Per-instance50x31 mapping roundtrip failed");
        check(get(b, "scale") == s && get(b, "root") == s % 12 && get(b, "chordThru") == s % 2, "Instance routing/fixed choice was lost");
    }
    const auto independent = get(b, FlysnapperProcessor::mappingParameterId(0));
    set(a, FlysnapperProcessor::mappingParameterId(0), float((int(independent) + 1) % 31));
    check(get(b, FlysnapperProcessor::mappingParameterId(0)) == independent, "Instances share assignment state");

    juce::MemoryBlock saved;
    a.getStateInformation(saved);
    auto xml = juce::AudioProcessor::getXmlFromBinary(saved.getData(), int(saved.getSize()));
    check(xml != nullptr, "Cannot inspect saved MIDI state");
    xml->setAttribute("schema", 1); // Existing schema-1 string IDs remain readable.
    auto* params = xml->getChildByName("FLYSNAPPER_PARAMETERS");
    check(params != nullptr, "Missing legacy parameter tree");
    for (const char* id : {"pitchEnabled", "dynamicsEnabled", "monoBend"}) {
        auto* legacy = params->createNewChildElement("PARAM");
        legacy->setAttribute("id", id); legacy->setAttribute("value", 1);
    }
    for (auto* child : params->getChildIterator())
        if (child->getStringAttribute("id").startsWith("map_")) child->setAttribute("value", 0);
    juce::MemoryBlock legacyBytes;
    juce::AudioProcessor::copyXmlToBinary(*xml, legacyBytes);
    b.setStateInformation(legacyBytes.getData(), int(legacyBytes.getSize()));
    for (int i = 0; i < 50; ++i)
        check(get(b, FlysnapperProcessor::mappingParameterId(i)) == get(a, FlysnapperProcessor::mappingParameterId(i)), "Stable scale/type IDs did not override the numeric cache");
    check(b.parameters.getParameter("pitchEnabled") == nullptr, "Legacy audio state resurrected a removed parameter");
    b.getStateInformation(saved);
    auto clean = juce::AudioProcessor::getXmlFromBinary(saved.getData(), int(saved.getSize()));
    check(clean != nullptr, "Cannot inspect resaved legacy state");
    for (auto* child : clean->getChildByName("FLYSNAPPER_PARAMETERS")->getChildIterator())
        check(!child->getStringAttribute("id").contains("pitchEnabled") && child->getStringAttribute("id") != "dynamicsEnabled"
              && child->getStringAttribute("id") != "monoBend", "Resaved MIDI-only state retained removed audio controls");
    const auto previousRoot = get(b, "root");
    xml->setAttribute("schema", 999);
    juce::AudioProcessor::copyXmlToBinary(*xml, legacyBytes);
    b.setStateInformation(legacyBytes.getData(), int(legacyBytes.getSize()));
    check(get(b, "root") == previousRoot, "Unknown schema changed instance state");
    feature("instance_state");
    passed("zero_audio_and_independent_50x31_state", "Processor declarations, all scale choices and stable-ID preset/instance copies; no DAW placement claim");
}

void normalisedChoiceCoverage() {
    Harness h("normalised_choice_coverage");
    auto& p = *h.processor;
    auto* scale = dynamic_cast<juce::AudioParameterChoice*>(p.parameters.getParameter("scale"));
    auto* root = dynamic_cast<juce::AudioParameterChoice*>(p.parameters.getParameter("root"));
    auto* mode = dynamic_cast<juce::AudioParameterChoice*>(p.parameters.getParameter("mode"));
    auto* mapping = dynamic_cast<juce::AudioParameterChoice*>(p.parameters.getParameter(FlysnapperProcessor::mappingParameterId(0)));
    check(scale && root && mode && mapping, "Missing discrete controls in normalised host-value coverage");
    for (int value = 0; value < 3; ++value) {
        mode->setValueNotifyingHost(float(value) / 2.0f);
        check(mode->getIndex() == value && get(p, "mode") == float(value), "Normalised mode choice loses its integer index");
    }
    mode->setValueNotifyingHost(1.0f);
    for (int s = 0; s < flysnapper::scaleCount; ++s) {
        scale->setValueNotifyingHost(float(s) / float(flysnapper::scaleCount - 1));
        check(scale->getIndex() == s && get(p, "scale") == float(s), "Normalised fixed-scale choice loses its integer index");
        for (int r = 0; r < 12; ++r) {
            root->setValueNotifyingHost(float(r) / 11.0f);
            check(root->getIndex() == r && get(p, "root") == float(r), "Normalised root choice loses its integer index");
            check(h.process(128).empty(), "A scale/root parameter change creates MIDI notes");
            const auto snapshot = p.getSnapshot();
            check(snapshot.effectiveScale == s && snapshot.effectiveRoot == r,
                  "Processed scale/root differs from the normalised host choices");
            check(snapshot.allowedMask == flysnapper::rotateMask(flysnapper::scaleDefinitions()[size_t(s)].mask, r),
                  "Processed allowed tones differ from the selected scale and root");
        }
    }
    set(p, "mode", 1);
    check(h.process(128, chord({48, 52, 55})).empty(), "Default last-stage chord filtering failed during mapping coverage");
    for (int s = 0; s < flysnapper::scaleCount; ++s) {
        mapping->setValueNotifyingHost(float(s) / float(flysnapper::scaleCount - 1));
        check(mapping->getIndex() == s && get(p, FlysnapperProcessor::mappingParameterId(0)) == float(s),
              "Normalised chord-scale choice loses its integer index");
        check(h.process(128).empty(), "Changing a chord assignment creates MIDI notes");
        const auto snapshot = p.getSnapshot();
        check(snapshot.typeIndex == 0 && snapshot.effectiveScale == s && snapshot.allowedMask == flysnapper::scaleDefinitions()[size_t(s)].mask,
              "Processed chord-scale assignment differs from the normalised host choice");
    }
    h.process(128, chord({48, 52, 55}, false));h.released();
    feature("normalised_choice_values");
    passed("normalised_choice_coverage", "Actual processor:31 normalised scale choices across12 roots, all3 modes,31 chord assignments and initial raw factory defaults; no synthetic fractional raw-cache injection");
}

void chordControlAndInternalBypass() {
    Harness h("chord_control_and_internal_bypass");
    auto& p = *h.processor;
    set(p, "mode", 0);
    Stream sameTime{{19, on(61, 1, 87)}};
    append(sameTime, chord({48, 52, 55}, true, 19));
    expectNote(h.process(128, sameTime), true, 60, 19, 1, 87);
    check(p.getSnapshot().chordMask == 0x091 && p.getSnapshot().root == 0, "Chord control was not analysed before the same-time playing note");
    expectNote(h.process(128, {{23, off(61)}}), false, 60, 23);
    set(p, "correction", 0);
    expectNote(h.process(128, {{5, on(61)}}), true, 61, 5);
    expectNote(h.process(128, {{8, off(61)}}), false, 61, 8);
    Stream change = chord({52, 55}, false, 11); // C remains held across C -> F/C.
    append(change, chord({53, 57}, true, 11));
    check(h.process(128, change).empty(), "Consumed chord controls leaked during internal bypass");
    check(p.getSnapshot().chordMask == 0x221 && p.getSnapshot().root == 5, "Internal bypass stopped chord tracking or lost the common held C");
    set(p, "correction", 1);
    expectNote(h.process(128, {{31, on(56)}}), true, 57, 31);
    expectNote(h.process(128, {{37, off(56)}}), false, 57, 37);
    h.process(128, chord({48, 53, 57}, false));
    h.released();
    feature("chord_channel_16"); feature("chord_filter"); feature("internal_correction_bypass");
    passed("chord_tracking_during_internal_off", "MIDI delivered to callbacks: channel16, same-sample priority and held notes; re-enable mid-chord needs no new chord event");
}

void threeStageChain() {
    // Deliberately different mathematical transforms: 63 ->64 ->65 ->66.
    // The hard-coded table proves cascade order, rather than comparing the
    // chain with another call to the implementation under test.
    constexpr std::array<int, 8> expected{{63, 64, 63, 65, 64, 64, 64, 66}};
    for (int mask = 0; mask < 8; ++mask) {
        Harness a("chain_" + std::to_string(mask) + "_a"), b("chain_" + std::to_string(mask) + "_b"), c("chain_" + std::to_string(mask) + "_c");
        for (auto* h : {&a, &b, &c}) set(*h->processor, "mode", 2);
        set(*a.processor, "scale", 1); set(*b.processor, "scale", 2); set(*c.processor, "scale", 1); set(*c.processor, "root", 2);
        set(*a.processor, "chordThru", 1); set(*b.processor, "chordThru", 1);
        set(*a.processor, "correction", float(mask & 1)); set(*b.processor, "correction", float((mask >> 1) & 1)); set(*c.processor, "correction", float((mask >> 2) & 1));
        auto input = chord({48, 52, 55}, true, 3); input.emplace_back(29, on(63, 1, 93));
        const auto aOut = a.process(128, input);
        sameStream(channel(aOut, 16), chord({48, 52, 55}, true, 3), "First instance did not relay chord bytes/offsets");
        const auto bOut = b.process(128, aOut);
        sameStream(channel(bOut, 16), chord({48, 52, 55}, true, 3), "Second instance did not relay chord bytes/offsets");
        expectNote(c.process(128, bOut), true, expected[size_t(mask)], 29, 1, 93);
        for (auto* h : {&a, &b, &c}) check(h->processor->getSnapshot().chordMask == 0x091, "An instance missed the relayed chord");
        // Each instance must retain its input->output binding after automation.
        for (auto* h : {&a, &b, &c}) { set(*h->processor, "correction", 0); set(*h->processor, "root", 11); }
        auto ending = chord({48, 52, 55}, false, 4); ending.emplace_back(47, off(63));
        const auto offOut = c.process(128, b.process(128, a.process(128, ending)));
        expectNote(offOut, false, expected[size_t(mask)], 47);
        a.released(); b.released(); c.released();
    }
    feature("serial_correction"); feature("chord_relay");
    passed("three_actual_processors_all_eight_internal_switch_patterns", "Explicit serial processor callbacks with channel16 delivery, fixed C-major/C-minor/D-major cascade and stable release; not a Live routing test");
}

void chordTimeAndHeldBindings() {
    Harness h("chord_time_and_held_bindings");
    auto& p = *h.processor; set(p, "mode", 0);
    Stream inside{{5, on(61)}};
    append(inside, chord({48, 52, 55}, true, 7)); inside.emplace_back(9, on(61));
    expectNote(h.process(128, inside), true, 60, 9);
    check(h.process(128, {{11, off(61)}}).empty(), "A suppressed note lost its FIFO binding");
    expectNote(h.process(128, {{13, on(61, 1, 0)}}), false, 60, 13);
    expectNote(h.process(128, {{17, on(61)}}), true, 60, 17);
    auto change = chord({48, 52, 55}, false, 3);
    append(change, chord({50, 54, 57}, true, 3));
    check(h.process(128, change).empty(), "Chord change retuned or ended an already sounding playing note");
    check(p.getSnapshot().root == 2 && p.getSnapshot().typeIndex == 0, "D-major chord was not recognised");
    set(p, "mode", 1); set(p, FlysnapperProcessor::mappingParameterId(0), 5); // D Dorian.
    expectNote(h.process(128, {{23, off(61)}}), false, 60, 23);
    expectNote(h.process(128, {{29, on(61)}}), true, 62, 29);
    set(p, "correction", 0);
    expectNote(h.process(128, {{31, off(61)}}), false, 62, 31);
    expectNote(h.process(128, {{37, on(61)}}), true, 61, 37);
    set(p, "correction", 1); set(p, "mode", 2); set(p, "scale", 1);
    expectNote(h.process(128, {{41, off(61)}}), false, 61, 41);
    h.process(128, chord({50, 54, 57}, false)); h.released();
    passed("no_chord_lookahead_and_stable_held_bindings", "A note before the chord remains suppressed; FIFO zero-velocity release and held notes survive chord, assignment, mode and correction changes");
}

void relayChangesAndConditionalBypass() {
    Harness a("relay_lifecycle_a"), b("relay_lifecycle_b");
    set(*a.processor, "mode", 0); set(*b.processor, "mode", 0);
    check(a.process(128, chord({48, 52, 55})).empty(), "Default last-stage chord filter failed");
    set(*a.processor, "chordThru", 1);
    auto start = a.process(128);
    Stream resetAndHeld{{0, juce::MidiMessage::controllerEvent(16, 123, 0)}};
    append(resetAndHeld, chord({48, 52, 55}));
    sameStream(channel(start, 16), resetAndHeld, "Enabling relay did not reset and replay the current held chord");
    check(b.process(128, start).empty() && b.processor->getSnapshot().chordMask == 0x091, "Downstream did not receive newly enabled relay state");
    set(*a.processor, "chordThru", 0);
    auto clear = a.process(128);
    check(clear.size() == 1 && clear.front().second.isController() && clear.front().second.getChannel() == 16
          && clear.front().second.getControllerNumber() == 123, "Disabling relay did not clear the downstream chord");
    b.process(128, clear);
    check(b.processor->getSnapshot().chordMask == 0 && a.processor->getSnapshot().chordMask == 0x091, "Relay switch corrupted local state or left downstream chord stale");
    set(*a.processor, "chordThru", 1);
    b.process(128, a.process(128));
    a.processor->requestPanic();
    b.process(128, a.process(128));
    check(a.processor->getSnapshot().chordMask == 0 && b.processor->getSnapshot().chordMask == 0, "Panic left a relayed chord in the chain");
    a.released(); b.released();

    Harness conditional("conditional_host_bypass_callback");
    set(*conditional.processor, "mode", 0);
    auto input = chord({48, 52, 55}, true, 9); input.emplace_back(17, on(61));
    expectNote(conditional.process(128, input, true), true, 61, 17);
    check(conditional.processor->getSnapshot().chordMask == 0x091, "Bypass callback stopped chord observation when actually invoked");
    expectNote(conditional.process(128, {{19, on(61)}}), true, 60, 19);
    auto release = conditional.process(128, {{29, off(61)}, {31, off(61)}});
    check(release.size() == 2, "Bypass callback lost an overlapping note binding");
    check(release[0].second.isNoteOff() && release[0].second.getNoteNumber() == 61 && release[0].first == 29, "Bypassed input FIFO release changed");
    check(release[1].second.isNoteOff() && release[1].second.getNoteNumber() == 60 && release[1].first == 31, "Corrected input FIFO release changed");
    conditional.process(128, chord({48, 52, 55}, false)); conditional.released();
    passed("relay_lifecycle_and_conditional_bypass_callback", "Relay handover/panic and processBlockBypassed only IF the host invokes it; no Device Activator or direct-endpoint failover claim");

    Harness downstream("ordinary_playing_host_skip_model");
    set(*downstream.processor, "mode", 2); set(*downstream.processor, "scale", 2);
    // No producer callback is invoked. This tests only ordinary playing bytes
    // supplied by this driver to the downstream processor. It does not model
    // delivery of a separately routed chord endpoint while its target is off.
    expectNote(downstream.process(128, {{13, on(63)}}), true, 63, 13);
    expectNote(downstream.process(128, {{37, off(63)}}), false, 63, 37); downstream.released();
    passed("ordinary_playing_host_skip_model", "Driver-supplied unprocessed playing MIDI reaches the next callback; deliberately not a Live/parallel chord-routing test");
}

void rawMessagesSustainAndPanic() {
    Harness h("raw_messages_sustain_and_panic");
    auto& p = *h.processor;
    set(p, "mode", 2); set(p, "scale", 1);
    Stream input;
    for (int ch = 2; ch <= 15; ++ch) {
        input.emplace_back(ch, on(61, ch, 73));
        input.emplace_back(20 + ch, juce::MidiMessage::controllerEvent(ch, 1, 92));
        input.emplace_back(40 + ch, juce::MidiMessage::pitchWheel(ch, 9351));
        input.emplace_back(60 + ch, juce::MidiMessage::channelPressureChange(ch, 51));
        input.emplace_back(80 + ch, off(61, ch));
    }
    std::stable_sort(input.begin(), input.end(), [](const Event& a, const Event& b) { return a.first < b.first; });
    sameStream(h.process(128, input), input, "Channels2–15 were corrected or lost expression");
    const juce::uint8 sysex[]{0x7d, 0x21, 0x31, 0x41};
    Stream system{{7, juce::MidiMessage::createSysExMessage(sysex, 4)}, {11, juce::MidiMessage::midiClock()}};
    sameStream(h.process(128, system), system, "Direct processor system messages changed");
    expectNote(h.process(128, {{13, on(61)}}), true, 62, 13);
    check(h.process(128, {{17, juce::MidiMessage::controllerEvent(1, 64, 127)}, {31, off(61)}}).empty(), "Internal sustain did not retain the corrected note");
    expectNote(h.process(128, {{41, juce::MidiMessage::controllerEvent(1, 64, 0)}}), false, 62, 41);
    Stream expression{{2, juce::MidiMessage::controllerEvent(1, 1, 61)}, {7, juce::MidiMessage::pitchWheel(1, 9812)}, {11, juce::MidiMessage::channelPressureChange(1, 47)}};
    sameStream(h.process(128, expression), expression, "Playing expression changed");
    set(p, "chordThru", 1);
    auto armed = chord({48, 52, 55});
    armed.emplace_back(3, on(61)); armed.emplace_back(7, on(67, 3));
    armed.emplace_back(11, juce::MidiMessage::controllerEvent(1, 66, 127));
    armed.emplace_back(13, juce::MidiMessage::controllerEvent(3, 64, 127));
    h.process(128, armed);
    p.requestPanic();
    const auto cleared = h.process(128);
    check(notes(cleared, true).empty(), "Panic creates a new note");
    const auto containsCC = [&](int ch, int cc, int value) {
        return std::any_of(cleared.begin(), cleared.end(), [&](const Event& e) { return e.second.isController() && e.second.getChannel() == ch
            && e.second.getControllerNumber() == cc && e.second.getControllerValue() == value; });
    };
    check(containsCC(1, 66, 0) && containsCC(3, 64, 0), "Panic leaves a forwarded hold pedal active");
    check(containsCC(16, 123, 0), "Panic does not clear relayed chord controls");
    check(p.getSnapshot().chordMask == 0, "Panic left the local chord");
    h.released();
    passed("raw_messages_pedals_and_all_channel_panic", "Direct processor pass-through for channels2–15/system/expression, core sustain and bounded note/pedal/chord cleanup; not a VST3 SysEx transport guarantee");
}

class Position final : public juce::AudioPlayHead {
public:
    int64_t sample = 0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p; p.setIsPlaying(playing); p.setTimeInSamples(sample); return p;
    }
};
void transportLimitsAndRealtime() {
    Harness h("transport_and_limits");
    auto& p = *h.processor;
    Position head; p.setPlayHead(&head);
    set(p, "mode", 2); set(p, "scale", 1);
    expectNote(h.process(128, {{17, on(61)}}), true, 62, 17);
    head.sample = 128; head.playing = false;
    expectNote(h.process(128), false, 62, 0);
    head.sample = 256; head.playing = true;
    expectNote(h.process(128, {{11, on(63)}}), true, 64, 11);
    head.sample = 4096;
    const auto seek = h.process(128);
    expectNote(seek, false, 64, 0);
    p.setPlayHead(nullptr); h.released();
    expectNote(h.process(128, {{19, on(61)}}), true, 62, 19);
    Stream excess;
    for (int i = 0; i < 2050; ++i) excess.emplace_back(i % 127, on(i % 128, 2));
    const auto saturated = h.process(128, excess);
    check(p.capacityFault() && p.droppedEvents() > 0, "Oversized input did not enter the documented capacity fault");
    check(notes(saturated, true).empty(), "Overflow published a partial batch of new notes");
    h.released();
    p.requestPanic(); h.process(128);
    check(!p.capacityFault(), "Explicit panic failed to recover input saturation");
    expectNote(h.process(128, {{43, on(61)}}), true, 62, 43);
    expectNote(h.process(128, {{57, off(61)}}), false, 62, 57); h.released();
    passed("transport_and_bounded_input_failure", "Actual processor stop/seek and oversized-event recovery retain causal offsets and terminate held notes");

    Harness realtime("preallocated_realtime_callback");
    set(*realtime.processor, "mode", 2); set(*realtime.processor, "scale", 1); set(*realtime.processor, "chordThru", 1);
    juce::AudioBuffer<float> audio(0, 128);
    juce::MidiBuffer midi; midi.ensureSize(262144);
    for (const auto& event : chord({48, 52, 55}, true, 3)) midi.addEvent(event.second, event.first);
    midi.addEvent(on(61), 17); midi.addEvent(off(61), 79);
    for (const auto& event : chord({48, 52, 55}, false, 97)) midi.addEvent(event.second, event.first);
    allocations = 0; watchAllocations = true;
    realtime.processor->processBlock(audio, midi);
    watchAllocations = false;
    check(allocations == 0, "The prepared MIDI callback allocates C++ objects");
    check(!realtime.processor->capacityFault(), "Ordinary preallocated callback overflowed");
    passed("preallocated_midi_callback", "Counts global C++ new/new[] only around one warmed, preallocated real processor callback; not an OS realtime or allocator-completeness certificate");
}

void routingPulseDoesNotOwnExternalNote() {
    Harness h("routing_pulse_source_isolation");
    set(*h.processor, "mode", 2); set(*h.processor, "scale", 0);
    expectNote(h.process(128, {{7, on(60)}}), true, 60, 7);
    h.processor->requestTestNote();
    check(h.process(128).empty(), "Diagnostic pulse retriggered an externally held common output");
    check(h.process(4800).empty(), "Diagnostic pulse release terminated the external note");
    expectNote(h.process(128, {{37, off(60)}}), false, 60, 37); h.released();
    set(*h.processor, "routingTest", 1);
    juce::MemoryBlock state; h.processor->getStateInformation(state);
    FlysnapperProcessor restored; restored.setStateInformation(state.getData(), int(state.getSize()));
    check(get(restored, "routingTest") == 0, "A preset re-arms the diagnostic test pulse");
    passed("diagnostic_source_isolation", "Deliberate MIDI routing test remains separate from external input and is never armed by preset restore; no audio detector exists");
}

void writeEvidence(const char* status, const juce::String& error = {}) {
    auto* object = new juce::DynamicObject;
    object->setProperty("schema", 1); object->setProperty("kind", "FlysnapperMidiOnlyProcessor");
    object->setProperty("status", status); object->setProperty("checks", int(checks)); object->setProperty("cases", cases);
    object->setProperty("events_file", "Flysnapper-midi-processor-events.csv"); object->setProperty("events_rows", juce::int64(eventRows));
    juce::Array<juce::var> rawCases;
    for (const auto& name : eventCases) rawCases.add(name);
    object->setProperty("event_cases", rawCases);
    object->setProperty("ableton_verified", false); object->setProperty("pitch_detection_implemented", false);
    object->setProperty("audio_input_channels", 0); object->setProperty("audio_output_channels", 0);
    object->setProperty("scale_count", flysnapper::scaleCount); object->setProperty("chord_type_count", flysnapper::chordTypeCount);
    juce::Array<juce::var> features;
    for (const auto& name : verifiedFeatures) features.add(name);
    object->setProperty("verified_features", features);
    object->setProperty("scope", "Actual processor callbacks and explicitly driver-connected chains; no Live12.4.6 Device Activator, track-channel routing, parallel delivery or plugin-placement verification");
    if (error.isNotEmpty()) object->setProperty("error", error);
    auto cwd = juce::File::getCurrentWorkingDirectory();
    if (!cwd.getChildFile("Flysnapper-midi-processor-events.csv").replaceWithText(juce::String::fromUTF8(csv.str().c_str())))
        throw std::runtime_error("Cannot save MIDI processor events");
    if (!cwd.getChildFile("Flysnapper-midi-processor-evaluation.json").replaceWithText(juce::JSON::toString(juce::var(object), true) + "\n"))
        throw std::runtime_error("Cannot save MIDI processor report");
}
}

int main() {
    csv << "case,rate,block,sample,offset,type,channel,note,value,bytes\n";
    try {
        juce::ScopedJuceInitialiser_GUI runtime;
        declarationsAndState(); normalisedChoiceCoverage(); chordControlAndInternalBypass(); threeStageChain(); chordTimeAndHeldBindings();
        relayChangesAndConditionalBypass(); rawMessagesSustainAndPanic(); transportLimitsAndRealtime(); routingPulseDoesNotOwnExternalNote();
        writeEvidence("passed");
        std::cout << "FlysnapperMidiOnlyProcessor: PASS (" << checks << " checks, " << cases.size() << " cases; no Ableton verification).\n";
        return 0;
    } catch (const std::exception& error) {
        watchAllocations = false;
        try { writeEvidence("failed", error.what()); } catch (...) {}
        std::cerr << "FlysnapperMidiOnlyProcessor: FAIL: " << error.what() << '\n';
        return 1;
    }
}
