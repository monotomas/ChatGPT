// SPDX-License-Identifier: AGPL-3.0-or-later
// MIDI-correction editor for the silent VST3 instrument adapter.
// Visual conventions follow the Buffkiller reference.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "Core/MusicData.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>

class FlysnapperProcessor;

class FlysnapperLook final : public juce::LookAndFeel_V4 {
public:
    FlysnapperLook();
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
};

// Scale menu order depends on the chord. Item IDs retain the parameter's
// stable scale index; JUCE's stock ComboBoxAttachment uses visible positions.
class FlysnapperScaleAttachment final : private juce::ComboBox::Listener {
public:
    FlysnapperScaleAttachment(juce::RangedAudioParameter&,juce::ComboBox&,
                             std::function<void()> onChange={});
    ~FlysnapperScaleAttachment() override;
private:
    void comboBoxChanged(juce::ComboBox*) override;
    juce::ComboBox& box;
    std::function<void()> changed;
    juce::ParameterAttachment attachment;
};

class FlysnapperMappingTable final : public juce::Component {
public:
    explicit FlysnapperMappingTable(FlysnapperProcessor&);
    void setHelp(bool);
    void resized() override;
    void paint(juce::Graphics&) override;
    static constexpr int rowHeight=86, chordColumnWidth=276;
private:
    void updateRow(size_t);
    std::array<juce::Label,flysnapper::chordTypeCount> names,chordTones,scaleTones,defaults;
    std::array<juce::ComboBox,flysnapper::chordTypeCount> choices;
    std::array<std::unique_ptr<FlysnapperScaleAttachment>,flysnapper::chordTypeCount> attachments;
    bool helpEnabled=true;
};

class FlysnapperEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit FlysnapperEditor(FlysnapperProcessor&);
    ~FlysnapperEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void setBackVisible(bool);
    void setGuideVisible(bool);
    void resetMappings();
    void showRoutingHelp();
    void addTip(juce::SettableTooltipClient&,const juce::String&);
    void setHelp(bool);
    float parameter(const char*) const;
    FlysnapperProcessor& processor;
    FlysnapperLook look;
    juce::TooltipWindow tooltips{this,400};
    juce::Component front,guidePanel;
    juce::Label title,byline,version,chord,activeScale,chordTones,chordStatus,allowedTitle;
    juce::Label midiTitle,modeTitle,rootTitle,scaleTitle,fixedScaleTones,midiRouting;
    juce::Label activity,eventStatus,warning,footer,backTitle,backDescription,backTip,guideTitle;
    std::array<juce::Label,12> notes;
    juce::ComboBox mode,root,scale;
    juce::ToggleButton correction{"Note Correction"},chordThru{"Chord MIDI Through"},help{"Help"};
    juce::TextButton flip{"Chord / scale setup"},panic{"Panic"},routingHelp{"MIDI routing"};
    juce::TextButton guide{"Guide"},guideClose{"Back to controls"},restore{"Reset all 52 to factory"};
    juce::TextEditor guideText;
    juce::Viewport viewport;
    FlysnapperMappingTable mappingTable;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> correctionAttachment,chordThruAttachment,helpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment,rootAttachment;
    std::unique_ptr<FlysnapperScaleAttachment> scaleAttachment;
    std::vector<std::pair<juce::SettableTooltipClient*,juce::String>> tips;
    bool backVisible=false,guideVisible=false,helpState=true;
    unsigned previousOutputCount=0;
    int activityTicks=0;
    juce::Rectangle<int> chordPanel,midiPanel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlysnapperEditor)
};
