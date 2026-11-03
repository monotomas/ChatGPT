// SPDX-License-Identifier: AGPL-3.0-or-later
// Palette and control styling adapted from the supplied Buffkiller reference.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <memory>

class FlysnapperProcessor;

class FlysnapperLook final : public juce::LookAndFeel_V4 {
public:
    FlysnapperLook();
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};

class FlysnapperInputMeter final : public juce::Component {
public:
    void setLevel(float decibels);
    void paint(juce::Graphics&) override;
private:
    float levelDb=-120.0f;
};

class FlysnapperMappingTable final : public juce::Component {
public:
    explicit FlysnapperMappingTable(FlysnapperProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    std::array<juce::Label, 50> names, defaults;
    std::array<juce::ComboBox, 50> choices;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 50> attachments;
};

class FlysnapperEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit FlysnapperEditor(FlysnapperProcessor&);
    ~FlysnapperEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void setBackVisible(bool);
    void resetMappings();
    void showRoutingHelp();
    float parameter(const char*) const;
    FlysnapperProcessor& processor;
    FlysnapperLook look;
    juce::TooltipWindow tooltips{this, 400};
    juce::Component front;
    juce::Label title, byline, version, chord, activeScale, chordStatus, allowedTitle;
    juce::Label pitchTitle, sidechain, detectedNotes, tuning, midiTitle, modeTitle, rootTitle, scaleTitle;
    juce::Label activity, eventStatus, warning, footer, backTitle, backDescription, midiRouting;
    juce::Label gateTitle, sensitivityTitle, releaseTitle;
    std::array<juce::Label, 12> notes;
    juce::ComboBox mode, root, scale, pitchMode, bendRange;
    juce::ToggleButton correction{"Correction enabled"}, pitchEnabled{"Audio to MIDI"}, monoBend{"Mono pitch bend"};
    juce::Slider inputGate, sensitivity, release;
    FlysnapperInputMeter inputMeter;
    juce::TextButton flip{"Chord / scale setup"}, panic{"Panic"};
    juce::TextButton routingHelp{"Audio / MIDI routing"};
    juce::TextButton restore{"Reset all 50 to factory"};
    juce::Viewport viewport;
    FlysnapperMappingTable mappingTable;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> correctionAttachment, pitchEnabledAttachment, monoBendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment, rootAttachment, scaleAttachment, pitchModeAttachment, bendRangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment, sensitivityAttachment, releaseAttachment;
    bool backVisible = false;
    unsigned previousOutputCount = 0;
    int activityTicks = 0;
    juce::Rectangle<int> chordPanel, pitchPanel, midiPanel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlysnapperEditor)
};
