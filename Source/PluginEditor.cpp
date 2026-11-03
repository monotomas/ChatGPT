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
    const auto bounds=juce::Rectangle<float>(float(x),float(y),float(width),float(height)).reduced(7.0f);
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
    setResizeLimits(900,760,1500,1100);
    label(title,"FLYSNAPPER",31.0f,text,true);
    label(byline,juce::String::fromUTF8("by Olav L\xc3\xa6greid"),13.0f,muted);
#ifdef JucePlugin_VersionString
    label(version,JucePlugin_VersionString,12.0f,muted);
#else
    label(version,"0.2.0",12.0f,muted);
#endif
    label(chord,"No chord input",44.0f,text,true);
    chord.setComponentID("chordDisplay");
    chord.setMinimumHorizontalScale(0.65f);
    label(activeScale,"Chromatic",18.0f,green);
    label(chordStatus,"Chord control: channel 16",13.0f,muted);
    label(allowedTitle,"ALLOWED NOTES",12.0f,muted,true);
    label(pitchTitle,"1  PITCH TO MIDI",16.0f,blue,true);
    label(sidechain,"Sidechain disabled",13.0f,muted);
    label(detectedNotes,"Detected notes: none",14.0f,blue);
    label(tuning,"A4 = 440 Hz",12.0f,muted);
    label(gateTitle,"INPUT THRESHOLD",11.0f,muted,true);
    label(sensitivityTitle,"SENSITIVITY",11.0f,muted,true);
    label(releaseTitle,"RELEASE",11.0f,muted,true);
    label(midiTitle,"2  MIDI TO MIDI",16.0f,orange,true);
    label(modeTitle,"MODE",12.0f,muted,true);
    label(rootTitle,"FIXED ROOT",12.0f,muted,true);
    label(scaleTitle,"FIXED SCALE",12.0f,muted,true);
    label(midiRouting,"Playing MIDI: channel 1\nChord control: channel 16",13.0f,muted);
    label(activity,"MIDI output: idle",13.0f,muted);
    label(eventStatus,"",12.0f,muted);
    label(warning,"Playing MIDI: 1   /   Chord control: 16   /   MPE off",13.0f,muted);
    label(footer,"AGPL-3.0-or-later  /  No warranty  /  See LICENSE.txt",11.0f,muted);
    label(backTitle,"CHORD / SCALE SETUP",23.0f,text,true);
    label(backDescription,"50 chord types. Your chosen scale applies in every root and is saved with this instance.",14.0f,muted);
    mode.addItem("Chord Notes",1);mode.addItem("Chord Scale",2);mode.addItem("Fixed Scale",3);
    pitchMode.addItem("Monophonic",1);pitchMode.addItem("Polyphonic",2);
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
    mode.setComponentID("correctionMode");root.setComponentID("fixedRoot");scale.setComponentID("fixedScale");
    correction.setComponentID("correctionEnabled");flip.setComponentID("flip");panic.setComponentID("panic");
    pitchEnabled.setComponentID("pitchEnabled");pitchMode.setComponentID("pitchMode");
    inputGate.setComponentID("inputGate");sensitivity.setComponentID("sensitivity");release.setComponentID("releaseMs");
    inputMeter.setComponentID("inputMeter");sidechain.setComponentID("audioStatus");detectedNotes.setComponentID("detectedNotes");
    inputGate.setName("Input threshold");sensitivity.setName("Sensitivity");release.setName("Release");inputMeter.setName("Sidechain input level");
    viewport.setComponentID("mappingViewport");
    mode.setTooltip("Chord Notes: received chord tones in every octave.\nChord Scale: your scale for the recognised chord type.\nFixed Scale: the root and scale below, independent of chord control.");
    correction.setTooltip("Off: incoming and detected notes keep their original pitches. Held notes retain their existing output binding.");
    pitchEnabled.setTooltip("Convert sidechain audio to MIDI. Keyboard MIDI continues independently when this is off.");
    pitchMode.setTooltip("Monophonic: one detected note at a time.\nPolyphonic: recognise several simultaneously sounding notes.");
    inputGate.setTooltip("Reject audio below this input level. Raise it to exclude background noise; lower it for quiet playing. Double-click to reset.");
    sensitivity.setTooltip("Detection sensitivity. Adjust for the input instrument and background noise. Double-click to reset.");
    release.setTooltip("Time used to confirm a detected note has ended. Shorter release gives faster stops; longer release can preserve quiet decays. Double-click to reset.");
    flip.setTooltip("Edit all 50 chord-to-scale choices. Flipping only changes this view; notes continue.");
    panic.setTooltip("Release Flysnapper's active notes and clear chord/history state.");
    restore.setTooltip("Restore all 50 chord-to-scale choices to factory settings. Other settings are unchanged.");
    sidechain.setTooltip("Select an audio source in the host's sidechain panel. An enabled bus alone cannot identify the source; silence does not mean disconnection.");
    warning.setTooltip("Use ordinary playing MIDI on channel 1 and direct chord MIDI on channel 16. MPE is not supported.");
    for(auto* item:{&title,&byline,&version,&warning,&footer})addAndMakeVisible(*item);
    addAndMakeVisible(flip);addAndMakeVisible(panic);addAndMakeVisible(front);
    front.setInterceptsMouseClicks(false,true);
    for(auto* item:{&chord,&activeScale,&chordStatus,&allowedTitle,&pitchTitle,&sidechain,&detectedNotes,&tuning,&gateTitle,&sensitivityTitle,&releaseTitle,&midiTitle,&modeTitle,&rootTitle,&scaleTitle,&midiRouting,&activity,&eventStatus})front.addAndMakeVisible(*item);
    for(auto* item:std::array<juce::Component*,10>{&mode,&root,&scale,&correction,&pitchEnabled,&pitchMode,&inputGate,&sensitivity,&release,&inputMeter})
        front.addAndMakeVisible(*item);
    for(size_t i=0;i<notes.size();++i){
        label(notes[i],noteNames[i],15.0f,muted,true);
        notes[i].setJustificationType(juce::Justification::centred);
        notes[i].setComponentID("allowed_"+juce::String(int(i)));
        front.addAndMakeVisible(notes[i]);
    }
    viewport.setViewedComponent(&mappingTable,false);viewport.setScrollBarsShown(true,false);
    addChildComponent(viewport);addChildComponent(backTitle);addChildComponent(backDescription);addChildComponent(restore);
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ChoiceAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    correctionAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"correction",correction);
    pitchEnabledAttachment=std::make_unique<ButtonAttachment>(processor.parameters,"pitchEnabled",pitchEnabled);
    modeAttachment=std::make_unique<ChoiceAttachment>(processor.parameters,"mode",mode);
    rootAttachment=std::make_unique<ChoiceAttachment>(processor.parameters,"root",root);
    scaleAttachment=std::make_unique<ChoiceAttachment>(processor.parameters,"scale",scale);
    pitchModeAttachment=std::make_unique<ChoiceAttachment>(processor.parameters,"pitchMode",pitchMode);
    gateAttachment=std::make_unique<SliderAttachment>(processor.parameters,"inputGate",inputGate);
    sensitivityAttachment=std::make_unique<SliderAttachment>(processor.parameters,"sensitivity",sensitivity);
    releaseAttachment=std::make_unique<SliderAttachment>(processor.parameters,"releaseMs",release);
    inputGate.setNumDecimalPlacesToDisplay(0);sensitivity.setNumDecimalPlacesToDisplay(2);release.setNumDecimalPlacesToDisplay(0);
    flip.onClick=[this]{setBackVisible(!backVisible);};
    panic.onClick=[this]{processor.requestPanic();};restore.onClick=[this]{resetMappings();};
    setSize(1040,760);timerCallback();startTimerHz(20);
}
FlysnapperEditor::~FlysnapperEditor(){stopTimer();setLookAndFeel(nullptr);}
float FlysnapperEditor::parameter(const char* id) const {
    if(auto* value=processor.parameters.getRawParameterValue(id))return value->load();
    return 0.0f;
}
void FlysnapperEditor::setBackVisible(bool shown){
    backVisible=shown;front.setVisible(!shown);
    viewport.setVisible(shown);backTitle.setVisible(shown);backDescription.setVisible(shown);restore.setVisible(shown);
    flip.setButtonText(shown?"Back to playing":"Chord / scale setup");resized();repaint();
}
void FlysnapperEditor::resetMappings(){
    const auto defaults=flysnapper::factoryDefaults();
    for(size_t i=0;i<defaults.size();++i)
        if(auto* p=processor.parameters.getParameter(FlysnapperProcessor::mappingParameterId(int(i)))){
            p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(float(defaults[i])));p->endChangeGesture();
        }
}
void FlysnapperEditor::timerCallback(){
    const auto snapshot=processor.getSnapshot();
    const auto description=juce::String::fromUTF8(flysnapper::chordName(snapshot.chordMask,snapshot.bass).c_str());
    chord.setText(snapshot.chordMask?description:"No chord input",juce::dontSendNotification);
    chord.setTooltip(juce::String::fromUTF8(flysnapper::chordDetail(snapshot.chordMask,snapshot.bass).c_str()));
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
    sidechain.setText(!pitchOn?"Audio to MIDI off":!connected?"Select a sidechain audio source":!finite||db<=-100.0f?"Sidechain enabled  /  input silent":"Sidechain enabled  /  "+juce::String(db,1)+" dBFS",juce::dontSendNotification);
    sidechain.setColour(juce::Label::textColourId,pitchOn&&connected?muted:blue);
    inputMeter.setLevel(connected&&finite?db:-120.0f);
    const bool adjustable=pitchOn;
    pitchMode.setEnabled(adjustable);inputGate.setEnabled(adjustable);sensitivity.setEnabled(adjustable);release.setEnabled(adjustable);
    juce::String detected;
    const auto detectedMask=processor.getDetectedMask();
    for(int i=0;i<12;++i)if(detectedMask&(1u<<i)){if(detected.isNotEmpty())detected+="  ";detected+=noteNames[i];}
    detectedNotes.setText(detected.isEmpty()?"Detected notes: none":"Detected notes: "+detected,juce::dontSendNotification);
    const auto count=processor.outputNotes();
    if(count!=previousOutputCount){activityTicks=8;previousOutputCount=count;}else if(activityTicks>0)--activityTicks;
    const bool fault=processor.capacityFault();
    activity.setText(fault?"Capacity exceeded - press Panic":activityTicks?"MIDI output: active":"MIDI output: idle",juce::dontSendNotification);
    activity.setColour(juce::Label::textColourId,fault?orange:activityTicks?green:muted);
    const auto rejected=processor.droppedEvents();
    eventStatus.setText(rejected?"Unsupported/dropped events: "+juce::String(rejected):"",juce::dontSendNotification);
    eventStatus.setColour(juce::Label::textColourId,orange);
}
void FlysnapperEditor::paint(juce::Graphics& g){
    g.fillAll(background);
    if(!backVisible){drawPanel(g,chordPanel,green);drawPanel(g,pitchPanel,blue);drawPanel(g,midiPanel,orange);}
    g.setColour(border);g.drawHorizontalLine(getHeight()-69,24.0f,float(getWidth()-24));
}
void FlysnapperEditor::resized(){
    const int w=getWidth(),h=getHeight();
    title.setBounds(24,17,270,40);byline.setBounds(295,29,180,22);
    version.setBounds(25,64,120,20);flip.setBounds(w-242,25,218,32);
    panic.setBounds(w-114,h-57,90,30);
    warning.setBounds(24,h-57,w-160,22);footer.setBounds(24,h-31,w-150,17);
    front.setBounds(getLocalBounds());
    chordPanel={24,104,w-48,207};pitchPanel={24,329,(w-66)/2,h-416};
    midiPanel={pitchPanel.getRight()+18,329,w-pitchPanel.getRight()-42,h-416};
    chord.setBounds(48,123,w-96,65);activeScale.setBounds(48,193,w-96,28);chordStatus.setBounds(48,229,w-96,22);
    allowedTitle.setBounds(48,266,110,20);
    const int chipsX=178,chipGap=6,chipW=(w-chipsX-50-11*chipGap)/12;
    for(size_t i=0;i<notes.size();++i)notes[i].setBounds(chipsX+int(i)*(chipW+chipGap),261,chipW,29);
    const int px=pitchPanel.getX()+26,py=pitchPanel.getY(),pw=pitchPanel.getWidth()-52;
    pitchTitle.setBounds(px+8,py+18,pw-8,25);
    const int modeWidth=std::min(174,pw/2-8);
    pitchEnabled.setBounds(px,py+54,pw-modeWidth-12,29);pitchMode.setBounds(px+pw-modeWidth,py+54,modeWidth,29);
    sidechain.setBounds(px,py+94,pw,21);inputMeter.setBounds(px,py+123,pw,12);detectedNotes.setBounds(px,py+147,pw,24);
    const int knobWidth=(pw-16)/3;
    for(int i=0;i<3;++i){
        const int x=px+i*(knobWidth+8);
        auto* caption=i==0?&gateTitle:i==1?&sensitivityTitle:&releaseTitle;
        auto* control=i==0?&inputGate:i==1?&sensitivity:&release;
        caption->setJustificationType(juce::Justification::centred);
        caption->setBounds(x,py+184,knobWidth,18);
        control->setBounds(x,py+205,knobWidth,103);
    }
    tuning.setBounds(px,py+pitchPanel.getHeight()-29,pw,19);
    const int mx=midiPanel.getX()+26,my=midiPanel.getY(),mw=midiPanel.getWidth()-52;
    midiTitle.setBounds(mx+8,my+18,mw-8,25);correction.setBounds(mx,my+54,mw,29);
    modeTitle.setBounds(mx,my+100,64,20);mode.setBounds(mx+76,my+95,mw-76,30);
    rootTitle.setBounds(mx,my+147,90,20);scaleTitle.setBounds(mx+108,my+147,mw-108,20);
    root.setBounds(mx,my+173,90,31);scale.setBounds(mx+108,my+173,mw-108,31);
    midiRouting.setBounds(mx,my+228,mw,47);
    activity.setBounds(mx,my+midiPanel.getHeight()-58,mw,22);eventStatus.setBounds(mx,my+midiPanel.getHeight()-33,mw,20);
    backTitle.setBounds(24,110,w-290,34);restore.setBounds(w-254,111,230,31);backDescription.setBounds(24,153,w-48,25);
    viewport.setBounds(24,193,w-48,h-280);mappingTable.setSize(viewport.getWidth()-viewport.getScrollBarThickness(),50*48);
}
