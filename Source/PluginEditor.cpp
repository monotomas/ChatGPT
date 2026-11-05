// SPDX-License-Identifier: AGPL-3.0-or-later
// Visual conventions adapted from the unchanged Buffkiller reference package.
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Core/MusicData.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour background{0xff141b22}, panel{0xff1d2731}, border{0xff344351};
const juce::Colour text{0xffedf2f5}, muted{0xff9badbb}, orange{0xffffb15d};
const juce::Colour blue{0xff5daeff}, green{0xff58d7bb};
constexpr const char* noteNames[]{"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
constexpr const char* dynamicsIds[]{"gateThresholdDb","gateHysteresisDb","gateReleaseMs",
    "gateAttackMs","gateHoldMs","inputGainDb","compressorThresholdDb","compressorRatio","limiterCeilingDb"};
constexpr const char* dynamicsNames[]{"GATE THRESHOLD","GATE HYSTERESIS","GATE RELEASE",
    "GATE ATTACK","GATE HOLD","INPUT GAIN","COMPRESSOR","RATIO","LIMITER CEILING"};
constexpr const char* dynamicsUnits[]{" dB"," dB"," ms"," ms"," ms"," dB"," dB",":1"," dB"};
constexpr double dynamicsDefaults[]{-60,8,160,.25,35,0,-24,3,-3};
constexpr const char* dynamicsTips[]{
    "Level needed to open the gate, measured after Input gain. Raise it to reject background noise and quiet handling sounds; lower it if soft notes fail to start. A wanted note and unwanted sound at the same level cannot always be separated by a gate.",
    "The gate closes this many dB below its opening threshold. More hysteresis keeps decaying strings open and prevents chatter, but also lets more noise through while a note fades.",
    "Time for the gate to fade after its hold ends. Longer preserves natural tails; shorter stops noise sooner. This is separate from the Tracking page's MIDI Release.",
    "How quickly the gate opens after a real input crosses the threshold. Short settings preserve pick and fret transients. Longer settings soften those transients and can delay recognition.",
    "Minimum time the gate stays open after its level falls below the closing threshold. A short hold prevents chatter; excessive hold admits noise between notes.",
    "Trim before the gate. It moves both wanted signal and existing noise relative to the gate threshold; it does not improve the signal-to-noise ratio. Leave headroom and learn again after a substantial change. Calibration uses this trim but does not change it.",
    "Compressor threshold. Signals above this level are reduced according to Ratio. Compression after the gate controls strong attacks without first lifting idle noise. It cannot recover a string buried in interference.",
    "Compression ratio above the compressor threshold. Higher values reduce the level difference between attacks and tails. Excessive compression can weaken attack cues used to distinguish new notes.",
    "Maximum conditioned level sent to pitch recognition. Lower it to leave headroom. This causal limiter has no lookahead buffer and adds no intentional waiting period. It cannot undo clipping already present at the input."};
void label(juce::Label& l, const juce::String& s, float size=14.0f, juce::Colour c=text, bool bold=false) {
    l.setText(s, juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)));
    l.setColour(juce::Label::textColourId, c);
    l.setMinimumHorizontalScale(1.0f);
    l.setBorderSize({0, 0, 0, 0});
}
void addScales(juce::ComboBox& box) {
    int i=1;
    for (const auto& definition : flysnapper::scaleDefinitions()) box.addItem(definition.name, i++);
}
void drawPanel(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour accent) {
    g.setColour(panel); g.fillRoundedRectangle(r.toFloat(), 10.0f);
    g.setColour(border); g.drawRoundedRectangle(r.toFloat(), 10.0f, 1.0f);
    g.setColour(accent); g.fillRoundedRectangle(float(r.getX()+18), float(r.getY()+20), 3.0f, 19.0f, 1.5f);
}
}

FlysnapperLook::FlysnapperLook() {
    setColour(juce::Label::textColourId, text);
    setColour(juce::ComboBox::backgroundColourId, background);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::arrowColourId, orange);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, border);
    setColour(juce::PopupMenu::highlightedTextColourId, orange);
    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, text);
    setColour(juce::TextButton::buttonOnColourId, orange.withAlpha(0.22f));
    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ToggleButton::tickColourId, orange);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff283644));
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, border);
    setColour(juce::ScrollBar::thumbColourId, border.brighter(0.3f));
}
void FlysnapperLook::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool down) {
    const float y=(button.getHeight()-18.0f)*0.5f;
    drawTickBox(g, button, 1.0f, y, 18.0f, 18.0f, button.getToggleState(), button.isEnabled(), over, down);
    g.setColour(text.withMultipliedAlpha(button.isEnabled()?1.0f:0.5f));
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawText(button.getButtonText(), 26, 0, button.getWidth()-26, button.getHeight(), juce::Justification::centredLeft);
}

void FlysnapperLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                    float position, float startAngle, float endAngle, juce::Slider& slider) {
    const bool compact=slider.getProperties().getWithDefault("compactDial",false);
    const auto bounds=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(compact?3.0f:7.0f);
    const float radius=std::min(bounds.getWidth(),bounds.getHeight())*0.5f;
    const float cx=bounds.getCentreX(), cy=bounds.getCentreY();
    auto body=juce::Rectangle<float>(cx-radius+5,cy-radius+5,2*radius-10,2*radius-10);
    g.setColour(juce::Colour(0xff10171d)); g.fillEllipse(body);
    g.setColour(border); g.drawEllipse(body,1.0f);
    juce::Path track; track.addCentredArc(cx,cy,radius,radius,0,startAngle,endAngle,true);
    g.strokePath(track,juce::PathStrokeType(3.0f));
    const auto accent=slider.isEnabled()?slider.findColour(juce::Slider::rotarySliderFillColourId):muted.darker();
    const float angle=startAngle+position*(endAngle-startAngle);
    juce::Path arc; arc.addCentredArc(cx,cy,radius,radius,0,startAngle,angle,true);
    g.setColour(accent); g.strokePath(arc,juce::PathStrokeType(3.0f));
    const float dx=std::sin(angle),dy=-std::cos(angle);
    g.drawLine(cx+dx*radius*0.32f,cy+dy*radius*0.32f,cx+dx*radius*0.73f,cy+dy*radius*0.73f,2.5f);
}
void FlysnapperInputMeter::setLevel(float decibels) {
    const float next=std::isfinite(decibels)?juce::jlimit(-120.0f,6.0f,decibels):-120.0f;
    if(std::abs(levelDb-next)>0.05f){levelDb=next;repaint();}
}
void FlysnapperInputMeter::paint(juce::Graphics& g) {
    const auto bounds=getLocalBounds().toFloat();
    g.setColour(background);g.fillRoundedRectangle(bounds,4.0f);
    const float fraction=juce::jlimit(0.0f,1.0f,(levelDb+80.0f)/80.0f);
    if(fraction>0.0f){g.setColour(levelDb>-6.0f?orange:blue);g.fillRoundedRectangle(bounds.withWidth(bounds.getWidth()*fraction),4.0f);}
    g.setColour(border);g.drawRoundedRectangle(bounds,4.0f,1.0f);
}

FlysnapperMappingTable::FlysnapperMappingTable(FlysnapperProcessor& processor) {
    const auto& types=flysnapper::chordTypes();
    const auto& scales=flysnapper::scaleDefinitions();
    for (size_t i=0; i<types.size(); ++i) {
        const auto& type=types[i];
        label(names[i], juce::String("C")+type.suffix, 16.0f, text, true);
        label(defaults[i], juce::String("Factory: ")+scales[size_t(type.defaultScale)].name, 12.0f, muted);
        addScales(choices[i]);
        choices[i].setComponentID("mapping_"+juce::String(type.id));
        choices[i].setTooltip("Applies to this chord type in every root. All 19 scales are available.\nFactory: "+juce::String(scales[size_t(type.defaultScale)].name));
        addAndMakeVisible(names[i]); addAndMakeVisible(defaults[i]); addAndMakeVisible(choices[i]);
        attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, FlysnapperProcessor::mappingParameterId(int(i)), choices[i]);
    }
    setSize(800, 50*48);
}
void FlysnapperMappingTable::setHelp(bool enabled) {
    const auto& types=flysnapper::chordTypes();
    const auto& scales=flysnapper::scaleDefinitions();
    for(size_t i=0;i<choices.size();++i)
        choices[i].setTooltip(enabled?"Applies to this chord type in every root. All 19 scales are available.\nFactory: "+juce::String(scales[size_t(types[i].defaultScale)].name):juce::String{});
}
void FlysnapperMappingTable::resized() {
    const int choiceX=std::max(165, getWidth()/4);
    const int choiceW=std::min(360, getWidth()/2);
    for (size_t i=0; i<names.size(); ++i) {
        const int y=int(i)*48;
        names[i].setBounds(16,y+8,choiceX-30,30);
        choices[i].setBounds(choiceX,y+8,choiceW,31);
        defaults[i].setBounds(choiceX+choiceW+18,y+8,getWidth()-choiceX-choiceW-30,31);
    }
}
void FlysnapperMappingTable::paint(juce::Graphics& g) {
    g.fillAll(panel);
    for (int i=0; i<50; ++i) {
        if (i%2==0) { g.setColour(background.withAlpha(0.35f)); g.fillRect(0,i*48,getWidth(),48); }
        g.setColour(border.withAlpha(0.5f)); g.drawHorizontalLine((i+1)*48-1, 12.0f, float(getWidth()-12));
    }
}

FlysnapperEditor::FlysnapperEditor(FlysnapperProcessor& p)
    : AudioProcessorEditor(p), processor(p), mappingTable(p) {
    setLookAndFeel(&look);
    setOpaque(true);
    setResizable(true,true);
    setResizeLimits(1000,900,1500,1200);
    label(title,"FLYSNAPPER",31.0f,text,true);
    label(byline,juce::String::fromUTF8("by Olav L\xc3\xa6greid"),13.0f,muted);
#ifdef JucePlugin_VersionString
    label(version,JucePlugin_VersionString,12.0f,muted);
#else
    label(version,"0.3.0",12.0f,muted);
#endif
    label(chord,"No chord input",44.0f,text,true);
    chord.setComponentID("chordDisplay");
    chord.setMinimumHorizontalScale(0.65f);
    label(activeScale,"Chromatic",18.0f,green);
    label(chordStatus,"Chord control: channel 16",13.0f,muted);
    label(allowedTitle,"ALLOWED NOTES",12.0f,muted,true);
    label(pitchTitle,"1  PITCH TO MIDI",16.0f,blue,true);
    label(sidechain,"Audio source: choose in host sidechain panel",13.0f,muted);
    label(detectedNotes,"Detected notes: none",14.0f,blue);
    label(tuning,"A4 = 440 Hz",12.0f,muted);
    label(gateTitle,"NEW NOTE THRESHOLD",11.0f,muted,true);
    label(sensitivityTitle,"SENSITIVITY",11.0f,muted,true);
    label(releaseTitle,"RELEASE",11.0f,muted,true);
    label(calibrationStatus,"Learn: 3 seconds quiet, then 17 seconds playing.",12.0f,muted);
    label(dynamicsSummary,"Input dynamics",12.0f,muted);
    label(guideTitle,"FLYSNAPPER GUIDE",23.0f,text,true);
    label(midiTitle,"2  MIDI TO MIDI",16.0f,orange,true);
    label(modeTitle,"MODE",12.0f,muted,true);
    label(rootTitle,"FIXED ROOT",12.0f,muted,true);
    label(scaleTitle,"FIXED SCALE",12.0f,muted,true);
    label(midiRouting,"Playing MIDI: channel 1\nChord control: channel 16",13.0f,muted);
    label(activity,"MIDI output: idle",13.0f,muted);
    label(eventStatus,"",12.0f,muted);
    label(warning,"External playing: 1   /   Chord control: 16",13.0f,muted);
    label(footer,"AGPL-3.0-or-later  /  No warranty  /  See LICENSE.txt",11.0f,muted);
    label(backTitle,"CHORD / SCALE SETUP",23.0f,text,true);
    label(backDescription,"50 chord types. Your chosen scale applies in every root and is saved with this instance.",14.0f,muted);
    mode.addItem("Chord Notes",1);mode.addItem("Chord Scale",2);mode.addItem("Fixed Scale",3);
    pitchMode.addItem("Monophonic",1);pitchMode.addItem("Polyphonic",2);
    bendRange.addItem("Range: +/-2 st",1);bendRange.addItem("Range: +/-12 st",2);
    for(int i=0;i<12;++i)root.addItem(noteNames[i],i+1);
    addScales(scale);
    auto knob=[](juce::Slider& control,const juce::String& suffix,double initial,juce::Colour accent){
        control.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        control.setTextBoxStyle(juce::Slider::TextBoxBelow,false,104,22);
        control.setTextValueSuffix(suffix);control.setDoubleClickReturnValue(true,initial);
        control.setColour(juce::Slider::rotarySliderFillColourId,accent);
        control.setColour(juce::Slider::textBoxTextColourId,text);
        control.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        control.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    };
    knob(inputGate," dB",-55.0,blue);knob(sensitivity,"",0.65,green);knob(release," ms",35.0,orange);
    for(size_t i=0;i<dynamicsKnobs.size();++i){
        label(dynamicsTitles[i],dynamicsNames[i],12.0f,muted,true);
        dynamicsTitles[i].setJustificationType(juce::Justification::centred);
        knob(dynamicsKnobs[i],dynamicsUnits[i],dynamicsDefaults[i],i<5?blue:i<8?green:orange);
        dynamicsKnobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow,false,104,18);
        dynamicsKnobs[i].getProperties().set("compactDial",true);
        dynamicsKnobs[i].setComponentID(dynamicsIds[i]);dynamicsKnobs[i].setName(dynamicsNames[i]);
        dynamicsKnobs[i].setNumDecimalPlacesToDisplay(i==3?2:(i==2||i==4)?0:1);
    }
    mode.setComponentID("correctionMode");root.setComponentID("fixedRoot");scale.setComponentID("fixedScale");
    correction.setComponentID("correctionEnabled");flip.setComponentID("flip");panic.setComponentID("panic");
    pitchEnabled.setComponentID("pitchEnabled");pitchMode.setComponentID("pitchMode");
    monoBend.setComponentID("monoBend");bendRange.setComponentID("bendRange");routingHelp.setComponentID("routingHelp");
    autoInput.setComponentID("autoInput");mpeOutput.setComponentID("mpeOutput");
    dynamicsEnabled.setComponentID("dynamicsEnabled");help.setComponentID("help");
    trackingTab.setComponentID("trackingTab");dynamicsTab.setComponentID("dynamicsTab");
    calibrate.setComponentID("inputCalibration");calibrationStatus.setComponentID("calibrationStatus");
    calibrationProgressBar.setComponentID("calibrationProgress");dynamicsSummary.setComponentID("dynamicsStatus");
    guide.setComponentID("guide");guideClose.setComponentID("guideClose");guidePanel.setComponentID("guidePanel");
    guideText.setComponentID("guideText");
    bendRange.setName("Mono pitch bend range");
    inputGate.setComponentID("inputGate");sensitivity.setComponentID("sensitivity");release.setComponentID("releaseMs");
    inputMeter.setComponentID("inputMeter");sidechain.setComponentID("audioStatus");detectedNotes.setComponentID("detectedNotes");
    tuning.setComponentID("bendStatus");
    inputGate.setName("New note threshold");sensitivity.setName("Sensitivity");release.setName("Release");inputMeter.setName("Sidechain input level");
    viewport.setComponentID("mappingViewport");
    mode.setTooltip("Chord Notes: received chord tones in every octave.\nChord Scale: your scale for the recognised chord type.\nFixed Scale: the root and scale below, independent of chord control.");
    correction.setTooltip("Off: incoming and detected notes keep their original pitches. Held notes retain their existing output binding.");
    pitchEnabled.setTooltip("Convert sidechain audio to MIDI. Keyboard MIDI continues independently when this is off.");
    pitchMode.setTooltip("Monophonic: one note with optional continuous bends and vibrato.\nPolyphonic: recognise simultaneous notes as discrete semitones. Polyphonic pitch bend is not generated.");
    monoBend.setTooltip("Follow guitar bends and vibrato in Monophonic mode. Audio notes and pitch wheel leave Flysnapper on channel 2; external channel-1 MIDI is preserved at this output. The host or instrument may combine channels or share pitch wheel. Use a dedicated audio receiver if keyboard notes bend with it. Match the receiver's bend range; MPE is not needed for ordinary mono bend.");
    autoInput.setTooltip("Use level-relative tonal/noise evidence and stationary mains reduction in pitch recognition. This is separate from the Input dynamics gate and the one-time Learn operation. The manual New note threshold applies when Automatic input is off. Sensitivity and MIDI Release remain available.");
    mpeOutput.setTooltip("Optional MIDI output format for an MPE-capable host and receiver. Mono follows continuous pitch with a +/-48-semitone member range. Poly sends discrete semitone notes without bends. Lower zone: master 1, members 2-15; channel 16 remains chord control. Legacy Mono pitch bend and Range controls apply when this is off.");
    bendRange.setTooltip("Receiver pitch-bend sensitivity: +/-2 or +/-12 semitones. Flysnapper sends the standard RPN range setting; if the instrument ignores it, set the same range in the instrument.");
    tuning.setTooltip("Audio-generated notes leave Flysnapper on channel 2 only while Mono pitch bend is active; otherwise they use channel 1. Match the receiver's bend range. Host channel merging or a shared instrument pitch wheel can also bend keyboard voices.");
    inputGate.setTooltip("Manual minimum level for a new detected note, used when Automatic input is off. Held notes may decay below this level while a reliable periodic signal remains. Double-click to reset.");
    sensitivity.setTooltip("Detection sensitivity. Adjust for the input instrument and background noise. Double-click to reset.");
    release.setTooltip("Time used to confirm a detected note has ended. Shorter release gives faster stops; longer release can preserve quiet decays. Double-click to reset.");
    flip.setTooltip("Edit all 50 chord-to-scale choices. Flipping only changes this view; notes continue.");
    panic.setTooltip("Release Flysnapper's active notes and clear chord/history state.");
    restore.setTooltip("Restore all 50 chord-to-scale choices to factory settings. Other settings are unchanged.");
    sidechain.setTooltip("Choose the audio source in the host's device sidechain panel, outside this plugin window. Use Audio / MIDI routing for instructions. An enabled bus alone cannot identify the source; silence does not mean disconnection.");
    routingHelp.setTooltip("Show where to choose the audio source and receive Flysnapper's MIDI output in the host.");
    warning.setTooltip("Use ordinary playing MIDI on channel 1 and direct chord MIDI on channel 16. MPE output requires the host and receiving instrument to preserve per-note expression.");
    dynamicsEnabled.setTooltip("Gate/expansion, compression and limiting before pitch recognition. Turn off to compare with the original input. The source audio is analysed only; Flysnapper's audio output stays silent.");
    calibrate.setTooltip("A 20-second guided sample: first mute the strings for 3 seconds while keeping your normal hand contact and posture. Then play soft/strong notes and chords for 17 seconds, including at least one long ring-out. On success the visible dynamics knobs are set once. Adjust them afterward. Click again during learning to cancel without applying a partial result.");
    calibrationStatus.setTooltip("Learning needs both a usable quiet section and representative playing. Use your normal pickup, guitar volume and audio-interface gain. Input clipping must be corrected before Flysnapper. Calibration cannot separate wanted and unwanted sounds with identical levels and spectra.");
    dynamicsSummary.setTooltip("State of the input dynamics gate and the conditioned level sent to recognition. The upper meter always shows the unprocessed sidechain level, so a closed gate does not hide an incoming signal.");
    trackingTab.setTooltip("Pitch recognition, mono bend and MIDI note release settings.");
    dynamicsTab.setTooltip("Condition the audio before recognition and learn starting settings from a 20-second playing sample.");
    help.setTooltip("Show or hide explanations when hovering over controls and their labels, like Buffkiller. The Guide remains available with hover help off.");
    guide.setTooltip("Read the built-in guide: calibration, dynamics, routing, scales and latency.");
    guideClose.setTooltip("Return to the controls. Audio and MIDI continue while you read the guide.");
    for(auto* item:{&title,&byline,&version,&warning,&footer})addAndMakeVisible(*item);
    addAndMakeVisible(flip);addAndMakeVisible(panic);addAndMakeVisible(routingHelp);addAndMakeVisible(help);addAndMakeVisible(guide);addAndMakeVisible(front);
    front.setInterceptsMouseClicks(false,true);
    for(auto* item:{&chord,&activeScale,&chordStatus,&allowedTitle,&pitchTitle,&sidechain,&detectedNotes,&tuning,&midiTitle,&modeTitle,&rootTitle,&scaleTitle,&midiRouting,&activity,&eventStatus,&dynamicsSummary})front.addAndMakeVisible(*item);
    for(auto* item:std::array<juce::Component*,8>{&mode,&root,&scale,&correction,&pitchEnabled,&pitchMode,&mpeOutput,&inputMeter})
        front.addAndMakeVisible(*item);
    front.addAndMakeVisible(trackingControls);front.addAndMakeVisible(dynamicsControls);
    front.addAndMakeVisible(trackingTab);front.addAndMakeVisible(dynamicsTab);
    trackingControls.setInterceptsMouseClicks(false,true);dynamicsControls.setInterceptsMouseClicks(false,true);
    for(auto* item:std::array<juce::Component*,9>{&monoBend,&bendRange,&autoInput,&inputGate,&sensitivity,&release,&gateTitle,&sensitivityTitle,&releaseTitle})trackingControls.addAndMakeVisible(*item);
    dynamicsControls.addAndMakeVisible(dynamicsEnabled);dynamicsControls.addAndMakeVisible(calibrate);
    dynamicsControls.addAndMakeVisible(calibrationStatus);dynamicsControls.addAndMakeVisible(calibrationProgressBar);
    calibrationProgressBar.setPercentageDisplay(false);
    calibrationProgressBar.setColour(juce::ProgressBar::backgroundColourId,background);
    calibrationProgressBar.setColour(juce::ProgressBar::foregroundColourId,green);
    for(size_t i=0;i<dynamicsKnobs.size();++i){dynamicsControls.addAndMakeVisible(dynamicsTitles[i]);dynamicsControls.addAndMakeVisible(dynamicsKnobs[i]);}
    for(size_t i=0;i<notes.size();++i){
        label(notes[i],noteNames[i],15.0f,muted,true);
        notes[i].setJustificationType(juce::Justification::centred);
        notes[i].setComponentID("allowed_"+juce::String(int(i)));
        front.addAndMakeVisible(notes[i]);
    }
    viewport.setViewedComponent(&mappingTable,false);viewport.setScrollBarsShown(true,false);
    addChildComponent(viewport);addChildComponent(backTitle);addChildComponent(backDescription);addChildComponent(restore);
    addChildComponent(guidePanel);guidePanel.addAndMakeVisible(guideTitle);guidePanel.addAndMakeVisible(guideClose);guidePanel.addAndMakeVisible(guideText);
    guidePanel.setInterceptsMouseClicks(false,true);
    guideText.setMultiLine(true,true);guideText.setReadOnly(true);guideText.setScrollbarsShown(true);
    guideText.setCaretVisible(false);guideText.setPopupMenuEnabled(true);
    guideText.setFont(juce::Font(juce::FontOptions(14.0f)));
    guideText.setColour(juce::TextEditor::backgroundColourId,panel);
    guideText.setColour(juce::TextEditor::textColourId,text);
    guideText.setColour(juce::TextEditor::outlineColourId,panel);
    guideText.setColour(juce::TextEditor::focusedOutlineColourId,panel);
    guideText.setText(
        "START WITH YOUR GUITAR\n\n"
        "Use your normal guitar volume, pickup and interface gain. Feed Flysnapper a clean direct signal when possible, before distortion, heavy compression or time effects. Set the interface gain high enough for a healthy signal, with headroom for your strongest attack. Increasing gain also increases existing noise; it cannot improve the signal-to-noise ratio.\n\n"
        "INPUT DYNAMICS / LEARN\n\n"
        "Open the Input dynamics tab and choose Learn / 20 seconds. First mute the strings for 3 seconds so Flysnapper can measure the background, while retaining your normal hand contact and playing posture. On an electric guitar, removing your hands can change grounding hum. Then play for 17 seconds: include soft and strong notes, at least one long ring-out and the chords you normally use. Follow the on-screen phase and remaining time. The host must be processing audio for the sample clock to advance.\n\n"
        "Audio-generated MIDI is muted during the sample and while the result is applied. External playing MIDI and chord control continue. Listen to the original guitar through your normal audio monitoring while learning. Cancel leaves the current dynamics settings intact. If the input is clipped or the sample does not contain usable quiet and playing sections, a failed sample leaves those settings intact too.\n\n"
        "After a successful sample, Flysnapper sets the visible gate and compressor controls once. Their new positions are the actual saved parameter values. You can adjust them immediately afterward; Learn is a starting point, not a continuous controller or a guarantee that every handling sound is rejected. Input gain is not increased automatically. Repeat the sample after a substantial pickup, guitar-volume, source-gain or noise change.\n\n"
        "GATE, EXPANSION, COMPRESSION AND LIMITING\n\n"
        "Gate threshold is the level required to admit a new sound. Raise it to reject more noise; lower it to admit softer playing. Hysteresis places the closing level below the opening level. A larger gap helps a note ring out without rapid gate chatter.\n\n"
        "Gate attack controls how quickly an accepted sound opens. A short attack preserves pick and fret detail. Hold prevents rapid closures around a fluctuating tail. Gate release fades the signal after it falls below the closing level; longer release preserves tails but also keeps background noise audible to the recogniser for longer. There is always a tradeoff when the wanted tail approaches the noise floor.\n\n"
        "Input gain trims the signal before the gate. It moves both wanted sound and existing noise relative to the thresholds, so revisit them or learn again after a substantial change. Calibration uses the current trim but does not change it. The compressor follows the gate and reduces strong levels above its threshold; Ratio controls how much. It can make the signal easier to handle, but strong compression can weaken useful attack cues. The limiter bounds the level sent to recognition without lookahead. Neither can repair input clipping or reliably separate wanted and unwanted sounds with the same acoustic evidence.\n\n"
        "The upper meter shows the original sidechain input. On the dynamics page, the status line shows the gate state and conditioned output. A closed gate therefore does not hide the fact that audio is arriving. Mono velocity retains the input attack strength before dynamics; poly velocity is estimated per recognised voice. These controls shape analysis audio and are not a separate MIDI velocity compressor. Flysnapper has no audio monitoring output.\n\n"
        "TRACKING AND NOTE ENDINGS\n\n"
        "Automatic input adds tonal/noise evidence and stationary-mains reduction. It observes input before the gate, so silence behind a closed gate cannot hide the background it needs to learn. It is separate from the dynamics controls and the one-time Learn sample. With Automatic input off, New note threshold supplies a manual minimum for new recognised notes. Sensitivity affects candidate acceptance. Changes to tracking setup or transport during learning can cancel the sample.\n\n"
        "Tracking Release is the time used to confirm that a recognised MIDI note has ended. It is distinct from Gate release, which changes the audio reaching recognition. If endings are too short, check both the gate closing level/release and Tracking Release. If notes stick or noise appears after damping, shorten the applicable release rather than simply extending all notes.\n\n"
        "Monophonic mode follows one source. Enable Mono pitch bend for smooth bends and vibrato; fretted slides and hammer-ons should still establish new notes. Polyphonic mode recognises simultaneous pitches as discrete semitones and does not generate polyphonic bending. Overlapping harmonics, very quiet strings and handling sounds remain more difficult than a clean isolated note.\n\n"
        "AUDIO / MIDI ROUTING\n\n"
        "In Ableton Live, choose the audio source in Flysnapper's own device box: open the host Sidechain panel outside this plugin window and select Audio From. A bus marked enabled is not proof of which track is connected.\n\n"
        "On the receiving MIDI track, select the Flysnapper track in the upper MIDI From field, then the Flysnapper plugin itself in the field below. Set Monitor to In and load the receiving instrument. For live audio, also make sure the source and Flysnapper tracks are monitored as intended. Flysnapper's own stereo audio output is silent.\n\n"
        "External playing MIDI uses channel 1; chord control must be routed manually to channel 16. Chord control is consumed and does not leave as playing notes. Notes held across a chord change remain in the chord analysis. Receive the plugin's MIDI output, rather than only its track's incoming MIDI.\n\n"
        "With ordinary Mono pitch bend active, generated notes use channel 2. Match the receiving instrument's bend range to the selected range if it ignores the transmitted RPN setting. External channel-1 MIDI remains separate at Flysnapper's output, but host routing or a receiver with a shared pitch wheel may combine expression. Use a dedicated receiver for the audio voice if needed.\n\n"
        "MPE output is optional and requires an MPE-capable host route and receiver. The lower zone uses master 1 and member channels 2-15 with a +/-48-semitone member bend range; channel 16 remains chord control. Mono follows continuous pitch in that format. Poly still sends discrete semitone notes.\n\n"
        "CORRECTION AND CHORD / SCALE SETUP\n\n"
        "Fixed Scale uses the root and scale shown on the front. Chord Notes allows the currently held chord pitch classes. Chord Scale uses the recognised chord type and its assigned scale. Open Chord / scale setup to choose independently for all 50 types; each choice transposes to the recognised root. All 19 scales are available.\n\n"
        "Correction applies when a new note is accepted. A held note retains its existing output binding when the chord changes. Empty or unrecognised chord control can suppress new notes in chord-dependent modes. Correction off passes the playing pitches through; it does not disable audio recognition.\n\n"
        "LATENCY AND HOST SETTINGS\n\n"
        "Pitch recognition needs some of the incoming waveform before deciding, especially for lower notes and overlapping sources. This decision time is distinct from the audio interface's buffer delay and from the plugin's reported latency samples. A zero reported delay is not an instantaneous audio-to-MIDI claim. Delivery notes report measured recognition results.\n\n"
        "Use the interface's native low-latency driver, a suitably small stable buffer, and direct routing for live playing. On Windows this usually means the device's own ASIO driver. Remove avoidable lookahead, linear-phase processing and other high-latency effects from the source, destination and monitored paths. Check the host's delay-compensation and monitoring options. Smaller buffers increase CPU demand; dropouts are not an improvement.\n\n"
        "Keep the guitar reasonably in tune, choose Mono for a single line, and use only as much gate hold/release and recognition release as the performance needs. A gate attack that is too long delays recognition; a threshold that is too high misses quiet notes. Faster settings can admit noise, so compare the complete musical result.\n\n"
        "PANIC AND HELP\n\n"
        "Panic releases active notes and clears chord/history state. Help enables or disables hover explanations on controls and labels. This Guide stays available either way. Opening the Guide or flipping the chord setup changes the view only; processing continues.\n",false);
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ChoiceAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    correctionAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"correction",correction);
    pitchEnabledAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"pitchEnabled",pitchEnabled);
    monoBendAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"monoBend",monoBend);
    autoInputAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"autoInput",autoInput);
    mpeOutputAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"mpeOutput",mpeOutput);
    dynamicsEnabledAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"dynamicsEnabled",dynamicsEnabled);
    helpAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"help",help);
    modeAttachment=std::make_unique<ChoiceAttachment>(processor.parameters,"mode",mode);
    rootAttachment=std::make_unique<ChoiceAttachment>(processor.parameters,"root",root);
    scaleAttachment=std::make_unique<ChoiceAttachment>(processor.parameters,"scale",scale);
    pitchModeAttachment=std::make_unique<ChoiceAttachment>(processor.parameters,"pitchMode",pitchMode);
    bendRangeAttachment=std::make_unique<ChoiceAttachment>(processor.parameters,"bendRange",bendRange);
    gateAttachment=std::make_unique<SliderAttachment>(processor.parameters,"inputGate",inputGate);
    sensitivityAttachment=std::make_unique<SliderAttachment>(processor.parameters,"sensitivity",sensitivity);
    releaseAttachment=std::make_unique<SliderAttachment>(processor.parameters,"releaseMs",release);
    for(size_t i=0;i<dynamicsKnobs.size();++i)
        dynamicsAttachments[i]=std::make_unique<SliderAttachment>(processor.parameters,dynamicsIds[i],dynamicsKnobs[i]);
    inputGate.setNumDecimalPlacesToDisplay(0);sensitivity.setNumDecimalPlacesToDisplay(2);release.setNumDecimalPlacesToDisplay(0);
    flip.onClick=[this]{setBackVisible(!backVisible);};
    panic.onClick=[this]{processor.requestPanic();};restore.onClick=[this]{resetMappings();};
    routingHelp.onClick=[this]{showRoutingHelp();};
    trackingTab.onClick=[this]{setDynamicsVisible(false);};dynamicsTab.onClick=[this]{setDynamicsVisible(true);};
    calibrate.onClick=[this]{
        const auto s=processor.getInputCalibrationSnapshot();
        using Phase=FlysnapperProcessor::InputCalibrationSnapshot::Phase;
        const bool active=s.pending||s.phase==Phase::Noise||s.phase==Phase::Playing||(s.phase==Phase::Complete&&!s.applied);
        if(active)processor.cancelInputCalibration();else processor.requestInputCalibration();
        timerCallback();
    };
    guide.onClick=[this]{setGuideVisible(!guideVisible);if(guideVisible)guideText.moveCaretToTop(false);};
    guideClose.onClick=[this]{setGuideVisible(false);};
    help.onClick=[this]{setHelp(help.getToggleState());};
    root.setTooltip("Root of Fixed Scale. Chord-dependent modes use the incoming chord instead.");
    scale.setTooltip("Choose any of the 19 fixed scales. Chord Scale uses the back-side choice for the recognised chord type.");
    for(auto* client:std::initializer_list<juce::SettableTooltipClient*>{&mode,&root,&scale,&correction,&pitchEnabled,&pitchMode,&monoBend,&autoInput,&mpeOutput,&bendRange,&tuning,&inputGate,&sensitivity,&release,&flip,&panic,&restore,&sidechain,&routingHelp,&warning,&dynamicsEnabled,&calibrate,&calibrationStatus,&dynamicsSummary,&trackingTab,&dynamicsTab,&help,&guide,&guideClose})
        addTip(*client,client->getTooltip());
    addTip(gateTitle,inputGate.getTooltip());addTip(sensitivityTitle,sensitivity.getTooltip());addTip(releaseTitle,release.getTooltip());
    addTip(modeTitle,mode.getTooltip());addTip(rootTitle,root.getTooltip());addTip(scaleTitle,scale.getTooltip());
    addTip(midiRouting,routingHelp.getTooltip());addTip(pitchTitle,pitchEnabled.getTooltip());
    for(size_t i=0;i<dynamicsKnobs.size();++i){addTip(dynamicsKnobs[i],dynamicsTips[i]);addTip(dynamicsTitles[i],dynamicsTips[i]);}
    for(auto& n:notes)addTip(n,"Allowed pitch classes for new corrected notes. These are calculated from the active mode, scale and held chord input.");
    setHelp(parameter("help")>=.5f);setDynamicsVisible(false);
    setSize(1040,900);timerCallback();startTimerHz(20);
}
FlysnapperEditor::~FlysnapperEditor(){stopTimer();setLookAndFeel(nullptr);}
float FlysnapperEditor::parameter(const char* id) const {
    if(auto* value=processor.parameters.getRawParameterValue(id))return value->load();
    return 0.0f;
}
void FlysnapperEditor::setBackVisible(bool shown){
    backVisible=shown;guideVisible=false;guidePanel.setVisible(false);guide.setToggleState(false,juce::dontSendNotification);front.setVisible(!shown);
    viewport.setVisible(shown);backTitle.setVisible(shown);backDescription.setVisible(shown);restore.setVisible(shown);
    flip.setButtonText(shown?"Back to playing":"Chord / scale setup");resized();repaint();
}
void FlysnapperEditor::setDynamicsVisible(bool shown){
    dynamicsVisible=shown;trackingControls.setVisible(!shown);dynamicsControls.setVisible(shown);
    detectedNotes.setVisible(!shown);dynamicsSummary.setVisible(shown);tuning.setVisible(!shown);
    trackingTab.setToggleState(!shown,juce::dontSendNotification);dynamicsTab.setToggleState(shown,juce::dontSendNotification);
    resized();repaint();
}
void FlysnapperEditor::setGuideVisible(bool shown){
    guideVisible=shown;guidePanel.setVisible(shown);guide.setToggleState(shown,juce::dontSendNotification);
    front.setVisible(!shown&&!backVisible);
    viewport.setVisible(!shown&&backVisible);backTitle.setVisible(!shown&&backVisible);
    backDescription.setVisible(!shown&&backVisible);restore.setVisible(!shown&&backVisible);
    resized();repaint();
}
void FlysnapperEditor::addTip(juce::SettableTooltipClient& client,const juce::String& value){tips.emplace_back(&client,value);client.setTooltip(value);}
void FlysnapperEditor::setHelp(bool enabled){
    helpState=enabled;mappingTable.setHelp(enabled);
    for(auto& item:tips)item.first->setTooltip(enabled?item.second:juce::String{});
}
void FlysnapperEditor::resetMappings(){
    const auto defaults=flysnapper::factoryDefaults();
    for(size_t i=0;i<defaults.size();++i)
        if(auto* p=processor.parameters.getParameter(FlysnapperProcessor::mappingParameterId(int(i)))){
            p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(float(defaults[i])));p->endChangeGesture();
        }
}
void FlysnapperEditor::showRoutingHelp(){
    setGuideVisible(true);
    guideText.setCaretPosition(std::max(0,guideText.getText().indexOf("AUDIO / MIDI ROUTING")));
}
void FlysnapperEditor::timerCallback(){
    const bool showHelp=parameter("help")>=.5f;if(showHelp!=helpState)setHelp(showHelp);
    const auto snapshot=processor.getSnapshot();
    const auto description=juce::String::fromUTF8(flysnapper::chordName(snapshot.chordMask,snapshot.bass).c_str());
    chord.setText(snapshot.chordMask?description:"No chord input",juce::dontSendNotification);
    chord.setTooltip(helpState?juce::String::fromUTF8(flysnapper::chordDetail(snapshot.chordMask,snapshot.bass).c_str()):juce::String{});
    const int selectedMode=juce::jlimit(0,2,snapshot.mode);
    const bool fixed=int(parameter("mode"))==2;
    root.setEnabled(fixed);scale.setEnabled(fixed);
    juce::String scaleText;
    if(!snapshot.correctionEnabled)scaleText="Correction bypassed";
    else if(selectedMode==0)scaleText="Chord Notes  /  incoming chord tones in every octave";
    else {
        const int scaleIndex=snapshot.effectiveScale,rootIndex=snapshot.effectiveRoot;
        scaleText=scaleIndex<0||scaleIndex>=19||rootIndex<0||rootIndex>=12?"Waiting for a recognised chord scale"
            :juce::String(noteNames[rootIndex])+" "+flysnapper::scaleDefinitions()[size_t(scaleIndex)].name;
    }
    activeScale.setText(scaleText,juce::dontSendNotification);
    chordStatus.setText(snapshot.candidateCount>1?juce::String(snapshot.candidateCount)+" possible names; first displayed candidate drives Chord Scale.":"Chord control: channel 16",juce::dontSendNotification);
    for(size_t i=0;i<notes.size();++i){
        const bool allowed=(snapshot.allowedMask&(1u<<i))!=0;
        notes[i].setColour(juce::Label::textColourId,allowed?text:muted.withAlpha(0.7f));
        notes[i].setColour(juce::Label::backgroundColourId,allowed?green.withAlpha(0.23f):background);
    }
    const bool pitchOn=parameter("pitchEnabled")>=0.5f;
    const bool connected=processor.sidechainActive();
    const float db=processor.getInputLevelDb();
    const bool finite=std::isfinite(db);
    sidechain.setText(!pitchOn?"Audio to MIDI off":!connected?"Audio source: choose in host sidechain panel":!finite||db<=-100.0f?"Sidechain enabled  /  input silent":"Sidechain enabled  /  "+juce::String(db,1)+" dBFS",juce::dontSendNotification);
    sidechain.setColour(juce::Label::textColourId,pitchOn&&connected?muted:blue);
    inputMeter.setLevel(connected&&finite?db:-120.0f);
    const bool adjustable=pitchOn;
    const bool automatic=parameter("autoInput")>=0.5f;
    const bool mpe=parameter("mpeOutput")>=0.5f;
    pitchMode.setEnabled(adjustable);autoInput.setEnabled(adjustable);
    inputGate.setEnabled(adjustable&&!automatic);sensitivity.setEnabled(adjustable);release.setEnabled(adjustable);
    const bool monoMode=int(parameter("pitchMode"))==0;
    const bool bends=parameter("monoBend")>=0.5f&&monoMode;
    monoBend.setEnabled(adjustable&&monoMode&&!mpe);bendRange.setEnabled(adjustable&&bends&&!mpe);
    const int range=int(parameter("bendRange"))==0?2:12;
    tuning.setText(mpe?(monoMode?"MPE mono bend  /  receiver: +/-48 st":"MPE output  /  Poly: discrete semitones"):bends?"Audio MIDI out: channel 2  /  receiver bend: +/-"+juce::String(range)+" st"
        :"A4 = 440 Hz  /  audio MIDI out: channel 1",juce::dontSendNotification);
    warning.setText(mpe?"MPE output  /  enable MPE in host and receiver  /  chord control: 16"
        :"External playing: 1   /   Chord control: 16",juce::dontSendNotification);
    midiRouting.setText("External playing: channel 1  /  chord control: 16\nReceive plugin MIDI via the host's MIDI From",juce::dontSendNotification);
    juce::String detected;
    const auto detectedMask=processor.getDetectedMask();
    for(int i=0;i<12;++i)if(detectedMask&(1u<<i)){if(detected.isNotEmpty())detected+="  ";detected+=noteNames[i];}
    detectedNotes.setText(detected.isEmpty()?"Detected notes: none":"Detected notes: "+detected,juce::dontSendNotification);
    const auto count=processor.outputEvents();
    if(count!=previousOutputCount){activityTicks=8;previousOutputCount=count;}else if(activityTicks>0)--activityTicks;
    const bool fault=processor.capacityFault();
    activity.setText(fault?"Capacity exceeded - press Panic":activityTicks?"MIDI output: active":"MIDI output: idle",juce::dontSendNotification);
    activity.setColour(juce::Label::textColourId,fault?orange:activityTicks?green:muted);
    const auto rejected=processor.droppedEvents();
    eventStatus.setText(rejected?"Unsupported/dropped events: "+juce::String(rejected):"",juce::dontSendNotification);
    eventStatus.setColour(juce::Label::textColourId,orange);
    const auto calibration=processor.getInputCalibrationSnapshot();
    using Phase=FlysnapperProcessor::InputCalibrationSnapshot::Phase;
    calibrationRunning=calibration.pending||calibration.phase==Phase::Noise||calibration.phase==Phase::Playing
        ||(calibration.phase==Phase::Complete&&!calibration.applied);
    calibrationProgress=std::isfinite(calibration.progress)?juce::jlimit(0.0,1.0,double(calibration.progress)):0;
    juce::String calibrationText;
    const int remaining=std::isfinite(calibration.secondsRemaining)?std::max(0,int(std::ceil(calibration.secondsRemaining))):0;
    const int quietRemaining=std::isfinite(calibration.secondsRemaining)?std::max(0,int(std::ceil(calibration.secondsRemaining-17.0f))):0;
    juce::Colour calibrationColour=muted;
    if(calibration.pending&&calibration.phase!=Phase::Noise&&calibration.phase!=Phase::Playing&&calibration.phase!=Phase::Complete)
        calibrationText="Waiting for host audio  /  Cancel to keep settings";
    else switch(calibration.phase){
        case Phase::Idle:calibrationText="Learn: 3 seconds quiet, then 17 seconds playing.";break;
        case Phase::Noise:calibrationText="Keep strings quiet  /  "+juce::String(quietRemaining)+" s left";calibrationColour=blue;break;
        case Phase::Playing:calibrationText="Play soft / strong notes and chords  /  "+juce::String(remaining)+" s";calibrationColour=green;break;
        case Phase::Complete:calibrationText=calibration.applied?"Learned settings applied. Adjust knobs freely.":"Applying learned settings...";calibrationColour=green;break;
        case Phase::Cancelled:calibrationText="Learning cancelled. Settings unchanged.";break;
        case Phase::Failed:
            calibrationColour=orange;
            switch(calibration.failureReason){
                case 1:calibrationText="No usable playing. Learn again; play after 3 s.";break;
                case 2:calibrationText="Too little quiet / note separation. Try again.";break;
                case 3:calibrationText="Input clipped. Lower source gain, then learn again.";break;
                case 4:calibrationText="Invalid input. Check source, then learn again.";break;
                case 5:calibrationText="Input unavailable. Check sidechain / host audio.";break;
                case 6:calibrationText="Settings changed. Learn again when ready.";break;
                default:calibrationText="Sample unusable. Settings unchanged; try again.";break;
            }break;
    }
    calibrationStatus.setText(calibrationText,juce::dontSendNotification);calibrationStatus.setColour(juce::Label::textColourId,calibrationColour);
    calibrate.setButtonText(calibrationRunning?"Cancel learning":"Learn / 20 seconds");
    calibrate.setEnabled(calibrationRunning||(pitchOn&&connected));
    dynamicsEnabled.setEnabled(pitchOn&&!calibrationRunning);
    const bool dynamicsOn=parameter("dynamicsEnabled")>=.5f;
    for(auto& knob:dynamicsKnobs)knob.setEnabled(pitchOn&&dynamicsOn&&!calibrationRunning);
    const float conditioned=processor.getDynamicsLevelDb(),gain=processor.getDynamicsGainDb();
    dynamicsSummary.setText(!dynamicsOn?"Input dynamics bypassed":juce::String(processor.isDynamicsGateOpen()?"Gate open":"Gate closed")
        +"  /  out "+juce::String(std::isfinite(conditioned)?conditioned:-120.0f,1)+" dBFS  /  gain "+juce::String(std::isfinite(gain)?gain:0.0f,1)+" dB",juce::dontSendNotification);
    dynamicsSummary.setColour(juce::Label::textColourId,dynamicsOn&&processor.isDynamicsGateOpen()?green:muted);
    if(helpState&&(calibration.phase==Phase::Complete||calibration.phase==Phase::Failed))
        calibrationStatus.setTooltip(calibrationText+"\nMeasured noise: "+juce::String(calibration.noiseFloorDb,1)+" dBFS; soft playing: "+juce::String(calibration.softLevelDb,1)
            +" dBFS; strong playing: "+juce::String(calibration.strongLevelDb,1)+" dBFS.\n"+(calibration.clipped?"Clipping was observed. Correct source gain before another sample.":"Current settings remain manually adjustable after learning."));
}
void FlysnapperEditor::paint(juce::Graphics& g){
    g.fillAll(background);
    if(guideVisible)drawPanel(g,{24,104,getWidth()-48,getHeight()-191},green);
    else if(!backVisible){drawPanel(g,chordPanel,green);drawPanel(g,pitchPanel,blue);drawPanel(g,midiPanel,orange);}
    g.setColour(border);g.drawHorizontalLine(getHeight()-69,24.0f,float(getWidth()-24));
}
void FlysnapperEditor::resized(){
    const int w=getWidth(),h=getHeight();
    title.setBounds(24,17,270,40);byline.setBounds(295,29,180,22);
    version.setBounds(25,64,120,20);routingHelp.setBounds(160,59,180,28);flip.setBounds(w-242,25,218,32);
    guide.setBounds(354,59,76,28);help.setBounds(445,59,82,28);
    panic.setBounds(w-114,h-57,90,30);
    warning.setBounds(24,h-57,w-160,22);footer.setBounds(24,h-31,w-150,17);
    front.setBounds(getLocalBounds());
    trackingControls.setBounds(getLocalBounds());dynamicsControls.setBounds(getLocalBounds());guidePanel.setBounds(getLocalBounds());
    chordPanel={24,104,w-48,175};pitchPanel={24,297,(w-66)/2,h-384};
    midiPanel={pitchPanel.getRight()+18,297,w-pitchPanel.getRight()-42,h-384};
    chord.setBounds(48,115,w-96,60);activeScale.setBounds(48,177,w-96,26);chordStatus.setBounds(48,207,w-96,22);
    allowedTitle.setBounds(48,243,110,20);
    const int chipsX=178,chipGap=6,chipW=(w-chipsX-50-11*chipGap)/12;
    for(size_t i=0;i<notes.size();++i)notes[i].setBounds(chipsX+int(i)*(chipW+chipGap),238,chipW,29);
    const int px=pitchPanel.getX()+26,py=pitchPanel.getY(),pw=pitchPanel.getWidth()-52;
    pitchTitle.setBounds(px+8,py+18,pw-8,25);
    const int modeWidth=std::min(174,pw/2-8);
    pitchEnabled.setBounds(px,py+50,pw-modeWidth-12,29);pitchMode.setBounds(px+pw-modeWidth,py+50,modeWidth,29);
    sidechain.setBounds(px,py+87,pw,21);inputMeter.setBounds(px,py+113,pw,12);detectedNotes.setBounds(px,py+130,pw,24);
    dynamicsSummary.setBounds(px,py+130,pw,24);
    const int tabWidth=(pw-8)/2;
    trackingTab.setBounds(px,py+164,tabWidth,29);dynamicsTab.setBounds(px+tabWidth+8,py+164,pw-tabWidth-8,29);
    const int rangeWidth=std::min(174,pw/2-8);
    monoBend.setBounds(px,py+208,pw-rangeWidth-12,29);bendRange.setBounds(px+pw-rangeWidth,py+208,rangeWidth,29);
    autoInput.setBounds(px,py+248,pw,29);
    const int knobWidth=(pw-16)/3;
    for(int i=0;i<3;++i){
        const int x=px+i*(knobWidth+8);
        auto* caption=i==0?&gateTitle:i==1?&sensitivityTitle:&releaseTitle;
        auto* control=i==0?&inputGate:i==1?&sensitivity:&release;
        caption->setJustificationType(juce::Justification::centred);
        caption->setBounds(x,py+289,knobWidth,18);
        control->setBounds(x,py+311,knobWidth,86);
    }
    dynamicsEnabled.setBounds(px,py+205,pw/2-6,29);calibrate.setBounds(px+pw/2+6,py+205,pw/2-6,29);
    calibrationStatus.setBounds(px,py+241,pw,20);calibrationProgressBar.setBounds(px,py+269,pw,7);
    const int dynamicsRow=std::max(74,(pitchPanel.getHeight()-294)/3);
    for(size_t i=0;i<dynamicsKnobs.size();++i){
        const int x=px+int(i%3)*(knobWidth+8),y=py+286+int(i/3)*dynamicsRow;
        dynamicsTitles[i].setBounds(x,y,knobWidth,14);
        dynamicsKnobs[i].setBounds(x,y+14,knobWidth,dynamicsRow-14);
    }
    tuning.setBounds(px,py+pitchPanel.getHeight()-29,pw,19);
    const int mx=midiPanel.getX()+26,my=midiPanel.getY(),mw=midiPanel.getWidth()-52;
    midiTitle.setBounds(mx+8,my+18,mw-8,25);correction.setBounds(mx,my+54,mw,29);
    modeTitle.setBounds(mx,my+100,64,20);mode.setBounds(mx+76,my+95,mw-76,30);
    rootTitle.setBounds(mx,my+147,90,20);scaleTitle.setBounds(mx+108,my+147,mw-108,20);
    root.setBounds(mx,my+173,90,31);scale.setBounds(mx+108,my+173,mw-108,31);
    midiRouting.setBounds(mx,my+228,mw,47);
    mpeOutput.setBounds(mx,my+285,mw,29);
    activity.setBounds(mx,my+midiPanel.getHeight()-58,mw,22);eventStatus.setBounds(mx,my+midiPanel.getHeight()-33,mw,20);
    backTitle.setBounds(24,110,w-290,34);restore.setBounds(w-254,111,230,31);backDescription.setBounds(24,153,w-48,25);
    viewport.setBounds(24,193,w-48,h-280);mappingTable.setSize(viewport.getWidth()-viewport.getScrollBarThickness(),50*48);
    guideTitle.setBounds(48,121,w-310,34);guideClose.setBounds(w-230,123,182,30);
    guideText.setBounds(48,171,w-96,h-280);
}
