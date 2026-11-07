// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Core/MusicData.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour background{0xff141b22},panel{0xff1d2731},border{0xff344351};
const juce::Colour text{0xffedf2f5},muted{0xff9badbb},orange{0xffffb15d};
const juce::Colour blue{0xff5daeff},green{0xff58d7bb};
constexpr const char* noteNames[]{"C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
constexpr const char* familyNames[]{"Chromatic","Diatonic modes","Melodic Minor modes","Bebop",
    "Harmonic Minor modes","Harmonic Major modes","Diminished","Whole Tone","Major Pentatonic","Minor Pentatonic","Minor Blues"};
constexpr int familyCount=int(sizeof(familyNames)/sizeof(familyNames[0]));
constexpr const char* assignmentTip="Tool tip: If different assignments are required for different parts of the tune, insert extra instances of Flysnapper to your track and automate Note Correction on/off. Chord MIDI must reach the chain on channel 16. Enable Chord MIDI Through on every instance except the last. When routing chord MIDI directly to the first instance, leave that instance enabled in Live.";
void label(juce::Label& l,const juce::String& s,float size=14,juce::Colour colour=text,bool bold=false){
    l.setText(s,juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions(size,bold?juce::Font::bold:juce::Font::plain)));
    l.setColour(juce::Label::textColourId,colour);l.setMinimumHorizontalScale(1.f);
    l.setBorderSize({0,0,0,0});l.setEditable(false,false,false);
}
juce::String namesFor(uint16_t mask){
    juce::StringArray names;for(int n=0;n<12;++n)if(mask&(1u<<n))names.add(noteNames[n]);
    return names.isEmpty()?juce::String("none"):names.joinIntoString("  ");
}
void populateScales(juce::ComboBox& box,uint16_t chordMask){
    box.clear(juce::dontSendNotification);
    const auto& definitions=flysnapper::scaleDefinitions();
    bool first=true,previousCompatible=true;int previousFamily=-1;
    for(int index:flysnapper::orderedScaleIndices(chordMask)){
        const auto& definition=definitions[size_t(index)];
        const bool compatible=flysnapper::scaleContainsChord(index,chordMask);
        if(chordMask!=0){
            if(first||compatible!=previousCompatible)
                box.addSectionHeading(compatible?"Scales with all chord tones":"Chord tones outside scale");
        }else if(definition.familyOrder!=previousFamily){
            const int family=std::clamp(definition.familyOrder,0,familyCount-1);box.addSectionHeading(familyNames[family]);
        }
        box.addItem((compatible?juce::String{}:juce::String("! "))+juce::String::fromUTF8(definition.name),index+1);
        previousCompatible=compatible;previousFamily=definition.familyOrder;first=false;
    }
    box.setScrollWheelEnabled(false);
}
void drawPanel(juce::Graphics& g,juce::Rectangle<int> r,juce::Colour accent){
    g.setColour(panel);g.fillRoundedRectangle(r.toFloat(),10.f);
    g.setColour(border);g.drawRoundedRectangle(r.toFloat(),10.f,1.f);
    g.setColour(accent);g.fillRoundedRectangle(float(r.getX()+18),float(r.getY()+18),3.f,20.f,1.5f);
}
}

FlysnapperLook::FlysnapperLook(){
    setColour(juce::Label::textColourId,text);
    setColour(juce::ComboBox::backgroundColourId,background);setColour(juce::ComboBox::textColourId,text);
    setColour(juce::ComboBox::outlineColourId,border);setColour(juce::ComboBox::arrowColourId,orange);
    setColour(juce::PopupMenu::backgroundColourId,panel);setColour(juce::PopupMenu::textColourId,text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,border);setColour(juce::PopupMenu::highlightedTextColourId,orange);
    setColour(juce::TextButton::buttonColourId,panel);setColour(juce::TextButton::textColourOffId,text);
    setColour(juce::TextButton::textColourOnId,text);setColour(juce::TextButton::buttonOnColourId,orange.withAlpha(.22f));
    setColour(juce::ToggleButton::textColourId,text);setColour(juce::ToggleButton::tickColourId,orange);
    setColour(juce::TooltipWindow::backgroundColourId,juce::Colour(0xff283644));
    setColour(juce::TooltipWindow::textColourId,text);setColour(juce::TooltipWindow::outlineColourId,border);
    setColour(juce::ScrollBar::thumbColourId,border.brighter(.3f));
}
void FlysnapperLook::drawToggleButton(juce::Graphics& g,juce::ToggleButton& button,bool over,bool down){
    const float y=(button.getHeight()-18.f)*.5f;
    drawTickBox(g,button,1.f,y,18.f,18.f,button.getToggleState(),button.isEnabled(),over,down);
    g.setColour(text.withMultipliedAlpha(button.isEnabled()?1.f:.5f));
    g.setFont(juce::Font(juce::FontOptions(14.f)));
    g.drawText(button.getButtonText(),26,0,button.getWidth()-26,button.getHeight(),juce::Justification::centredLeft);
}

FlysnapperScaleAttachment::FlysnapperScaleAttachment(juce::RangedAudioParameter& parameter,juce::ComboBox& control,std::function<void()> onChange)
    :box(control),changed(std::move(onChange)),attachment(parameter,[this](float value){
        box.setSelectedId(std::clamp(juce::roundToInt(value),0,flysnapper::scaleCount-1)+1,juce::dontSendNotification);
        if(changed)changed();
    }){
    box.addListener(this);attachment.sendInitialUpdate();
}
FlysnapperScaleAttachment::~FlysnapperScaleAttachment(){box.removeListener(this);}
void FlysnapperScaleAttachment::comboBoxChanged(juce::ComboBox*){
    const int index=box.getSelectedId()-1;
    if(index>=0&&index<flysnapper::scaleCount)attachment.setValueAsCompleteGesture(float(index));
}

FlysnapperMappingTable::FlysnapperMappingTable(FlysnapperProcessor& processor){
    const auto& types=flysnapper::chordTypes();
    for(size_t i=0;i<types.size();++i){
        const auto& type=types[i];const auto id=juce::String(type.id);
        label(names[i],juce::String("C")+type.suffix,16.f,text,true);names[i].setComponentID("chordName_"+id);
        label(chordTones[i],"Chord tones: "+namesFor(flysnapper::chordToneMask(int(i))),13.f,muted);
        chordTones[i].setComponentID("chordTones_"+id);chordTones[i].setJustificationType(juce::Justification::topLeft);
        label(scaleTones[i],{},13.f,green);scaleTones[i].setComponentID("scaleTones_"+id);
        label(defaults[i],"Factory: "+juce::String::fromUTF8(flysnapper::scaleDefinitions()[size_t(type.defaultScale)].name),11.5f,muted);
        defaults[i].setComponentID("factory_"+id);choices[i].setComponentID("mapping_"+id);
        populateScales(choices[i],flysnapper::chordToneMask(int(i)));
        addAndMakeVisible(names[i]);addAndMakeVisible(chordTones[i]);addAndMakeVisible(choices[i]);
        addAndMakeVisible(scaleTones[i]);addAndMakeVisible(defaults[i]);
        if(auto* parameter=processor.parameters.getParameter(FlysnapperProcessor::mappingParameterId(int(i))))
            attachments[i]=std::make_unique<FlysnapperScaleAttachment>(*parameter,choices[i],[this,i]{updateRow(i);});
        updateRow(i);
    }
    setSize(1000,int(types.size())*rowHeight);
}
void FlysnapperMappingTable::updateRow(size_t row){
    const int selected=choices[row].getSelectedId()-1;
    if(selected<0||selected>=flysnapper::scaleCount)return;
    const auto chordMask=flysnapper::chordToneMask(int(row));
    const auto& selectedScale=flysnapper::scaleDefinitions()[size_t(selected)];
    const auto missing=uint16_t(chordMask&~selectedScale.mask);
    juce::String description="Scale tones: "+namesFor(selectedScale.mask);
    if(missing)description+="    Outside scale: "+namesFor(missing);
    scaleTones[row].setText(description,juce::dontSendNotification);
    scaleTones[row].setColour(juce::Label::textColourId,missing?orange:green);
    juce::String tip="This scale type applies in the actual chord's root and is saved with this instance.\n";
    tip+="Chord tones in C: "+namesFor(chordMask)+"\n"+description;
    tip+=missing?"\n! This scale omits chord tones. The choice remains available.":"\nThis scale contains all nominal chord tones.";
    choices[row].setTooltip(helpEnabled?tip:juce::String{});
    chordTones[row].setTooltip(helpEnabled?"Nominal chord-type formula in C. Recognition can allow some tones to be omitted; the formula itself is not editable.":juce::String{});
    scaleTones[row].setTooltip(helpEnabled?description:juce::String{});
    defaults[row].setTooltip(helpEnabled?"Reset all 52 restores these factory assignments in this instance only.":juce::String{});
}
void FlysnapperMappingTable::setHelp(bool enabled){helpEnabled=enabled;for(size_t i=0;i<choices.size();++i)updateRow(i);}
void FlysnapperMappingTable::resized(){
    const int choiceX=chordColumnWidth+18,choiceWidth=std::max(240,getWidth()-choiceX-18);
    for(size_t i=0;i<names.size();++i){
        const int y=int(i)*rowHeight;
        names[i].setBounds(16,y+10,chordColumnWidth-24,24);
        chordTones[i].setBounds(16,y+38,chordColumnWidth-24,36);
        choices[i].setBounds(choiceX,y+9,choiceWidth,31);
        scaleTones[i].setBounds(choiceX,y+44,choiceWidth,19);
        defaults[i].setBounds(choiceX,y+66,choiceWidth,16);
    }
}
void FlysnapperMappingTable::paint(juce::Graphics& g){
    g.fillAll(panel);
    for(size_t i=0;i<names.size();++i){
        if(i%2==0){g.setColour(background.withAlpha(.35f));g.fillRect(0,int(i)*rowHeight,getWidth(),rowHeight);}
        g.setColour(border.withAlpha(.5f));g.drawHorizontalLine((int(i)+1)*rowHeight-1,12.f,float(getWidth()-12));
    }
}

FlysnapperEditor::FlysnapperEditor(FlysnapperProcessor& p):AudioProcessorEditor(p),processor(p),mappingTable(p){
    setLookAndFeel(&look);setOpaque(true);setResizable(true,true);setResizeLimits(1000,720,1600,1300);
    label(title,"FLYSNAPPER",31.f,text,true);label(byline,juce::String::fromUTF8("by Olav L\xc3\xa6greid"),13.f,muted);
#ifdef JucePlugin_VersionString
    label(version,JucePlugin_VersionString,12.f,muted);
#else
    label(version,"0.4.0",12.f,muted);
#endif
    label(chord,"No chord input",42.f,text,true);chord.setComponentID("chordDisplay");chord.setMinimumHorizontalScale(.75f);
    label(activeScale,"Chromatic",18.f,green);activeScale.setComponentID("activeScale");
    label(chordTones,"Chord tones: none",14.f,muted);chordTones.setComponentID("receivedChordTones");
    label(chordStatus,"Chord control: channel 16",13.f,muted);label(allowedTitle,"ALLOWED NOTES",12.f,muted,true);
    label(midiTitle,"MIDI TO MIDI",16.f,orange,true);label(modeTitle,"MODE",12.f,muted,true);
    label(rootTitle,"FIXED ROOT",12.f,muted,true);label(scaleTitle,"FIXED SCALE",12.f,muted,true);
    label(fixedScaleTones,{},14.f,green);fixedScaleTones.setComponentID("fixedScaleTones");
    label(midiRouting,"Playing MIDI: channel 1    /    Chord control: channel 16",14.f,muted);
    label(activity,"MIDI output: idle",13.f,muted);label(eventStatus,{},12.f,muted);
    label(warning,"Note Correction off keeps chord analysis active.",12.f,muted);
    label(footer,"AGPL-3.0-or-later  /  See LICENSE.txt",11.f,muted);
    label(backTitle,"CHORD / SCALE SETUP",23.f,text,true);
    label(backDescription,"52 chord types, here assigned to root C. Your chosen scale type applies in the actual chord's root and is auto-saved with this instance.",14.f,text,true);
    // Use the typographic apostrophe required by the supplied UI wording.
    backDescription.setText(juce::String::fromUTF8("52 chord types, here assigned to root C. Your chosen scale type applies in the actual chord\xe2\x80\x99s root and is auto-saved with this instance."),juce::dontSendNotification);
    backDescription.setComponentID("backDescription");backDescription.setJustificationType(juce::Justification::topLeft);
    label(backTip,assignmentTip,13.f,muted);backTip.setComponentID("assignmentTip");backTip.setJustificationType(juce::Justification::topLeft);
    label(guideTitle,"FLYSNAPPER GUIDE",23.f,text,true);
    mode.addItem("Chord Tones Only",1);mode.addItem("Chord-Scale Assignment",2);mode.addItem("Fixed Scale",3);
    for(int n=0;n<12;++n)root.addItem(noteNames[n],n+1);populateScales(scale,0);
    mode.setComponentID("correctionMode");root.setComponentID("fixedRoot");scale.setComponentID("fixedScale");
    correction.setComponentID("correctionEnabled");chordThru.setComponentID("chordThru");help.setComponentID("help");
    flip.setComponentID("flip");panic.setComponentID("panic");routingHelp.setComponentID("routingHelp");
    viewport.setComponentID("mappingViewport");restore.setComponentID("resetMappings");
    guide.setComponentID("guide");guideClose.setComponentID("guideClose");guidePanel.setComponentID("guidePanel");guideText.setComponentID("guideText");
    addAndMakeVisible(front);
    for(auto* c:std::initializer_list<juce::Component*>{&chord,&activeScale,&chordTones,&chordStatus,&allowedTitle,&midiTitle,&modeTitle,&rootTitle,&scaleTitle,&fixedScaleTones,&midiRouting,&activity,&eventStatus,&mode,&root,&scale,&correction,&chordThru})front.addAndMakeVisible(c);
    for(int n=0;n<12;++n){label(notes[size_t(n)],noteNames[n],13.f,muted,true);notes[size_t(n)].setJustificationType(juce::Justification::centred);front.addAndMakeVisible(notes[size_t(n)]);}
    viewport.setViewedComponent(&mappingTable,false);addChildComponent(viewport);addChildComponent(backTitle);addChildComponent(backDescription);addChildComponent(backTip);addChildComponent(restore);
    addChildComponent(guidePanel);guidePanel.addAndMakeVisible(guideTitle);guidePanel.addAndMakeVisible(guideClose);guidePanel.addAndMakeVisible(guideText);
    guideText.setMultiLine(true);guideText.setReadOnly(true);guideText.setScrollbarsShown(true);guideText.setCaretVisible(false);guideText.setPopupMenuEnabled(false);
    guideText.setColour(juce::TextEditor::backgroundColourId,panel);guideText.setColour(juce::TextEditor::textColourId,text);guideText.setColour(juce::TextEditor::outlineColourId,border);
    guideText.setFont(juce::Font(juce::FontOptions(15.f)));
    guideText.setText(R"GUIDE(MIDI ROUTING
Playing notes arrive on channel 1. Chord-control notes arrive on channel 16. Other channels pass without note correction. The host decides whether a route enters the track's chain or directly addresses a plugin; these are different routing methods.

For a serial chain, enable Chord MIDI Through on intermediate Flysnapper instances and turn it off on the last instance. The last instance removes chord-control messages from its MIDI output so they do not play the destination instrument. Each active correction stage receives the previous stage's corrected playing MIDI.

NOTE CORRECTION AND DEVICE ACTIVATION
Automating Note Correction on/off preserves the instance's chord analysis. With correction off, playing notes pass unchanged while chord-control messages are still followed. Already-held playing notes keep the output binding with which they began, so later Note Offs release the correct notes.

The host's Device Activator may stop the plugin's processing entirely. If chord MIDI is routed directly to that instance, deactivation must not be assumed to redirect the feed to another instance. An inactive instance cannot be assumed to receive chord changes or to be resent all held chord notes when reactivated. Track-input routing and channel preservation must be checked in the actual host. No routing-specific host result is implied by these controls.

CORRECTION MODES
Chord Tones Only corrects to the received chord's held pitch classes. Chord-Scale Assignment uses the scale assigned to the recognised chord type on the back. Fixed Scale uses the selected root and scale independently of chord input.

The nearest allowed MIDI note is selected without waiting for future notes. Equidistant choices follow the established melodic-direction rules, with upward correction as the fallback. Chord changes affect new notes; held playing-note bindings remain stable. Empty or unrecognised chord input cannot supply an invented scale.

CHOOSING SCALES
All 38 scales remain selectable. Each chord-type menu first lists Scales with all chord tones, then Chord tones outside scale. Within each section the requested parent-family and mode order is retained. Harmonic Major modes follow Harmonic Minor modes and precede Diminished. An exclamation mark identifies a scale that omits nominal chord tones; the row names the missing tones. The choice is allowed and no hidden chord/scale union is added.

Chord-type and mapping scale tones on the back are displayed relative to C. The selected scale type transposes to the actual incoming chord root. Melodic Minor uses its ascending form in both directions. Pitch-class names may have enharmonic equivalents.

PRESETS AND INSTANCES
Every instance saves its own 52 assignments, fixed scale and controls. Copying a device copies its saved settings; subsequent edits remain independent. Reset all 52 restores this instance's factory assignments. It does not reset another instance.

HELP, PANIC AND TIMING
Help enables contextual tooltips. Guide and front/back changes do not alter MIDI. Panic releases tracked playing notes and clears chord analysis. The MIDI routing test sends a brief C note; it is a routing check, not a played performance.

Flysnapper adds no intentional note-analysis delay. Host buffers, routing and the receiving instrument can still contribute to audible latency. This MIDI-only version contains no audio input or pitch detector.
)GUIDE",false);
    for(auto* c:std::initializer_list<juce::Component*>{&title,&byline,&version,&flip,&panic,&routingHelp,&help,&guide,&warning,&footer})addAndMakeVisible(c);
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    correctionAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"correction",correction);
    chordThruAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"chordThru",chordThru);
    helpAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"help",help);
    modeAttachment=std::make_unique<ComboAttachment>(processor.parameters,"mode",mode);
    rootAttachment=std::make_unique<ComboAttachment>(processor.parameters,"root",root);
    if(auto* parameter=processor.parameters.getParameter("scale"))scaleAttachment=std::make_unique<FlysnapperScaleAttachment>(*parameter,scale,[this]{timerCallback();});
    flip.onClick=[this]{setBackVisible(!backVisible);};panic.onClick=[this]{processor.requestPanic();};restore.onClick=[this]{resetMappings();};
    routingHelp.onClick=[this]{showRoutingHelp();};guide.onClick=[this]{setGuideVisible(!guideVisible);};guideClose.onClick=[this]{setGuideVisible(false);};help.onClick=[this]{setHelp(help.getToggleState());};
    addTip(mode,"Chord Tones Only uses held chord tones. Chord-Scale Assignment uses the mapping for the recognised chord type. Fixed Scale ignores chord input when choosing allowed notes.");
    addTip(root,"Root of Fixed Scale. Chord-dependent modes use the received chord's root.");
    addTip(scale,"All 38 fixed scales, ordered by parent family and mode. The selection is saved with this instance.");
    addTip(correction,"Automate this control to enable or disable note correction while continuing to receive chord changes. Existing held notes retain their original output binding.");
    addTip(chordThru,"Forward channel-16 chord control unchanged to a following Flysnapper. Enable on intermediate instances; leave off on the last instance to keep chord-control notes out of the receiving instrument.");
    addTip(flip,"Flip to all 52 independent chord/scale assignments. Editing the view itself does not change MIDI.");
    addTip(panic,"Release tracked notes and pedals and clear the current chord. Use this if playback was interrupted.");
    addTip(restore,"Restore all 52 factory choices in this instance only.");
    addTip(routingHelp,"Read MIDI routing guidance and the distinction between track-input routing, direct-device routing and device activation.");
    addTip(help,"Enable or disable contextual tooltips. The built-in guide remains available.");addTip(guide,"Open the built-in MIDI-only guide.");addTip(guideClose,"Return to the previous controls.");
    for(auto& n:notes)addTip(n,"Allowed pitch classes for new corrected notes in the current mode.");
    setHelp(parameter("help")>=.5f);
    setSize(1040,760);timerCallback();startTimerHz(20);
}
FlysnapperEditor::~FlysnapperEditor(){stopTimer();viewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr);}
float FlysnapperEditor::parameter(const char* id)const{
    // A parameter attachment can run before APVTS updates its cached raw
    // value: parameter listeners are notified newest first. Read the actual
    // parameter for immediate, message-thread control previews.
    if(auto* p=processor.parameters.getParameter(id))return p->convertFrom0to1(p->getValue());
    return 0.f;
}
void FlysnapperEditor::addTip(juce::SettableTooltipClient& client,const juce::String& value){tips.emplace_back(&client,value);}
void FlysnapperEditor::setHelp(bool enabled){helpState=enabled;for(auto& tip:tips)tip.first->setTooltip(enabled?tip.second:juce::String{});mappingTable.setHelp(enabled);}
void FlysnapperEditor::setBackVisible(bool shown){
    backVisible=shown;guideVisible=false;guidePanel.setVisible(false);guide.setToggleState(false,juce::dontSendNotification);
    front.setVisible(!shown);viewport.setVisible(shown);backTitle.setVisible(shown);backDescription.setVisible(shown);backTip.setVisible(shown);restore.setVisible(shown);
    flip.setButtonText(shown?"Back to playing":"Chord / scale setup");resized();repaint();
}
void FlysnapperEditor::setGuideVisible(bool shown){
    guideVisible=shown;guidePanel.setVisible(shown);guide.setToggleState(shown,juce::dontSendNotification);
    front.setVisible(!shown&&!backVisible);viewport.setVisible(!shown&&backVisible);
    backTitle.setVisible(!shown&&backVisible);backDescription.setVisible(!shown&&backVisible);backTip.setVisible(!shown&&backVisible);restore.setVisible(!shown&&backVisible);repaint();
}
void FlysnapperEditor::showRoutingHelp(){setGuideVisible(true);guideText.moveCaretToTop(false);}
void FlysnapperEditor::resetMappings(){
    const auto defaults=flysnapper::factoryDefaults();
    for(int i=0;i<flysnapper::chordTypeCount;++i)if(auto* p=processor.parameters.getParameter(FlysnapperProcessor::mappingParameterId(i))){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(float(defaults[size_t(i)])));p->endChangeGesture();}
}
void FlysnapperEditor::timerCallback(){
    const bool currentHelp=parameter("help")>=.5f;if(currentHelp!=helpState)setHelp(currentHelp);
    const auto snapshot=processor.getSnapshot();
    const auto chordText=flysnapper::formatChord(snapshot);
    chord.setText(snapshot.chordMask==0?juce::String("No chord input"):chordText.empty()?juce::String("Unrecognised chord"):juce::String::fromUTF8(chordText.c_str()),juce::dontSendNotification);
    chordTones.setText("Chord tones: "+namesFor(snapshot.chordMask),juce::dontSendNotification);
    const int currentMode=std::clamp(juce::roundToInt(parameter("mode")),0,2);
    const int selected=std::clamp(juce::roundToInt(parameter("scale")),0,flysnapper::scaleCount-1);
    const int selectedRoot=std::clamp(juce::roundToInt(parameter("root")),0,11);
    const bool fixed=currentMode==2;root.setEnabled(fixed);scale.setEnabled(fixed);rootTitle.setEnabled(fixed);scaleTitle.setEnabled(fixed);
    // Preview the current controls even while the host is stopped and has not
    // processed a block. Recognition still comes from the last input snapshot.
    int effectiveScale=fixed?selected:-1,effectiveRoot=fixed?selectedRoot:snapshot.root;
    if(currentMode==1&&snapshot.typeIndex>=0&&snapshot.typeIndex<flysnapper::chordTypeCount)
        if(auto* value=processor.parameters.getParameter(FlysnapperProcessor::mappingParameterId(snapshot.typeIndex)))
            effectiveScale=std::clamp(juce::roundToInt(value->convertFrom0to1(value->getValue())),0,flysnapper::scaleCount-1);
    const bool enabled=parameter("correction")>=.5f;
    juce::String scaleText;
    if(!enabled)scaleText="Note Correction off";
    else if(currentMode==0)scaleText="Chord Tones Only";
    else if(effectiveScale>=0&&effectiveRoot>=0){
        scaleText=juce::String::fromUTF8(flysnapper::scaleDefinitions()[size_t(effectiveScale)].name);
        scaleText="Root "+juce::String(noteNames[effectiveRoot%12])+"  /  "+scaleText;
    }else scaleText="Waiting for a recognised chord";
    activeScale.setText(scaleText,juce::dontSendNotification);
    fixedScaleTones.setText("Fixed scale tones: "+namesFor(flysnapper::rotateMask(flysnapper::scaleDefinitions()[size_t(selected)].mask,selectedRoot)),juce::dontSendNotification);
    fixedScaleTones.setColour(juce::Label::textColourId,fixed?green:muted);
    chordStatus.setText("Chord control: channel 16  /  "+juce::String(snapshot.candidateCount>1?"recognition has alternatives":snapshot.typeIndex>=0?"recognised":"awaiting chord")+(parameter("chordThru")>=.5f?"  /  forwarded":"  /  consumed here"),juce::dontSendNotification);
    uint16_t previewMask=!enabled?0xfff:currentMode==0?snapshot.chordMask:0;
    if(enabled&&effectiveScale>=0&&effectiveRoot>=0)
        previewMask=flysnapper::rotateMask(flysnapper::scaleDefinitions()[size_t(effectiveScale)].mask,effectiveRoot);
    for(int n=0;n<12;++n){const bool allowed=(previewMask&(1u<<n))!=0;notes[size_t(n)].setColour(juce::Label::textColourId,allowed?background:muted);notes[size_t(n)].setColour(juce::Label::backgroundColourId,allowed?green:background);}
    const auto count=processor.outputNotes();if(count!=previousOutputCount){activityTicks=8;previousOutputCount=count;}else activityTicks=std::max(0,activityTicks-1);
    activity.setText(activityTicks>0?"MIDI output: active":"MIDI output: idle",juce::dontSendNotification);activity.setColour(juce::Label::textColourId,activityTicks>0?green:muted);
    eventStatus.setText("Output events: "+juce::String(processor.outputEvents())+"    Dropped input events: "+juce::String(processor.droppedEvents()),juce::dontSendNotification);
    warning.setText(processor.capacityFault()?"MIDI capacity reached: tracked notes were released. Reduce the incoming event density.":"Note Correction off keeps chord analysis active.",juce::dontSendNotification);
    warning.setColour(juce::Label::textColourId,processor.capacityFault()?orange:muted);
}
void FlysnapperEditor::paint(juce::Graphics& g){
    g.fillAll(background);g.setColour(border);g.drawHorizontalLine(87,24.f,float(getWidth()-24));
    if(!backVisible&&!guideVisible){drawPanel(g,chordPanel,green);drawPanel(g,midiPanel,orange);}
    if(backVisible&&!guideVisible){g.setColour(panel);g.fillRoundedRectangle(viewport.getBounds().toFloat(),10.f);}
    if(guideVisible){g.setColour(panel);g.fillRoundedRectangle(guidePanel.getBounds().toFloat(),10.f);}
}
void FlysnapperEditor::resized(){
    const int w=getWidth(),h=getHeight();
    title.setBounds(24,20,235,40);byline.setBounds(26,61,280,18);version.setBounds(257,34,65,20);
    flip.setBounds(w-224,27,200,32);guide.setBounds(w-310,27,76,32);help.setBounds(w-386,29,66,29);
    panic.setBounds(w-472,27,76,32);routingHelp.setBounds(w-598,27,116,32);
    warning.setBounds(24,h-53,w-48,20);footer.setBounds(24,h-28,w-48,17);
    front.setBounds(getLocalBounds());chordPanel={24,105,w-48,205};midiPanel={24,328,w-48,h-401};
    chord.setBounds(48,116,w-96,60);activeScale.setBounds(48,180,w-96,28);chordTones.setBounds(48,214,w-96,23);chordStatus.setBounds(48,243,w-96,20);
    allowedTitle.setBounds(48,279,110,18);
    const int chipsX=178,gap=6,chipWidth=(w-chipsX-50-11*gap)/12;
    for(int n=0;n<12;++n)notes[size_t(n)].setBounds(chipsX+n*(chipWidth+gap),273,chipWidth,29);
    midiTitle.setBounds(56,344,210,24);correction.setBounds(48,380,(w-96)/2,29);chordThru.setBounds(w/2+12,380,w/2-60,29);
    modeTitle.setBounds(48,429,70,20);mode.setBounds(128,423,w-176,32);
    rootTitle.setBounds(48,473,96,20);scaleTitle.setBounds(160,473,w-208,20);
    root.setBounds(48,500,96,32);scale.setBounds(160,500,w-208,32);
    fixedScaleTones.setBounds(48,545,w-96,23);midiRouting.setBounds(48,575,w-96,23);
    activity.setBounds(48,h-118,220,22);eventStatus.setBounds(294,h-118,w-342,22);
    backTitle.setBounds(24,109,w-290,33);restore.setBounds(w-254,110,230,31);
    backDescription.setBounds(24,154,w-48,43);backTip.setBounds(24,205,w-48,66);
    viewport.setBounds(24,282,w-48,h-352);mappingTable.setSize(viewport.getWidth()-viewport.getScrollBarThickness(),flysnapper::chordTypeCount*FlysnapperMappingTable::rowHeight);
    guidePanel.setBounds(24,105,w-48,h-175);guideTitle.setBounds(24,15,w-350,34);guideClose.setBounds(w-278,17,206,31);guideText.setBounds(24,66,w-96,h-266);
}
