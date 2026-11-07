// SPDX-License-Identifier: AGPL-3.0-or-later
// Actual MIDI-only processor/editor integration; not an Ableton execution claim.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

namespace {
thread_local bool countNew=false;thread_local unsigned newCalls=0;
void* allocate(size_t n){if(countNew)++newCalls;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
}
void* operator new(size_t n){return allocate(n);}void* operator new[](size_t n){return allocate(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}void operator delete[](void* p,size_t)noexcept{std::free(p);}
namespace {
unsigned assertions=0;
void check(bool value,const char* message){++assertions;if(!value)throw std::runtime_error(message);}
void set(FlysnapperProcessor& p,const juce::String& id,float value){auto* parameter=p.parameters.getParameter(id);check(parameter!=nullptr,"missing parameter");parameter->setValueNotifyingHost(parameter->convertTo0to1(value));}
float get(FlysnapperProcessor& p,const juce::String& id){auto* value=p.parameters.getRawParameterValue(id);check(value!=nullptr,"missing raw parameter");return value->load();}
using Midi=std::vector<std::pair<int,juce::MidiMessage>>;
Midi process(FlysnapperProcessor& p,int samples,const Midi& input={},bool bypass=false){
    juce::AudioBuffer<float> audio(0,samples);juce::MidiBuffer midi;
    for(const auto& e:input)midi.addEvent(e.second,e.first);
    if(bypass)p.processBlockBypassed(audio,midi);else p.processBlock(audio,midi);
    Midi out;for(const auto e:midi)out.emplace_back(e.samplePosition,e.getMessage());return out;
}
juce::MidiMessage on(int note,int channel=1,int velocity=100){return juce::MidiMessage::noteOn(channel,note,juce::uint8(velocity));}
juce::MidiMessage off(int note,int channel=1){return juce::MidiMessage::noteOff(channel,note);}
Midi cChord(int offset=0){return {{offset,on(48,16)},{offset,on(52,16)},{offset,on(55,16)}};}
void saveLoad(FlysnapperProcessor& from,FlysnapperProcessor& to){juce::MemoryBlock block;from.getStateInformation(block);check(block.getSize()>0,"state empty");to.setStateInformation(block.getData(),int(block.getSize()));}
std::unique_ptr<juce::XmlElement> stateXml(FlysnapperProcessor& p){juce::MemoryBlock b;p.getStateInformation(b);return juce::AudioProcessor::getXmlFromBinary(b.getData(),int(b.getSize()));}
void loadXml(FlysnapperProcessor& p,const juce::XmlElement& xml){juce::MemoryBlock b;juce::AudioProcessor::copyXmlToBinary(xml,b);p.setStateInformation(b.getData(),int(b.getSize()));}
juce::Component* find(juce::Component& p,const juce::String& id){if(p.getComponentID()==id)return &p;for(int n=0;n<p.getNumChildComponents();++n)if(auto* c=find(*p.getChildComponent(n),id))return c;return nullptr;}
void screenshot(juce::AudioProcessorEditor& editor,const char* name){
    const auto image=editor.createComponentSnapshot(editor.getLocalBounds());juce::FileOutputStream out(juce::File::getCurrentWorkingDirectory().getChildFile(name));
    check(out.openedOk(),"screenshot file failed");out.setPosition(0);out.truncate();check(juce::PNGImageFormat().writeImageToStream(image,out),"PNG snapshot failed");
}
void settleGuiTimers(const std::function<bool()>& ready){
    // Console tests do not run a host message loop. Dispatch due GUI timers
    // on the actual message thread rather than capturing a pre-refresh label.
    for(int attempt=0;attempt<200;++attempt){
        juce::Timer::callPendingTimersSynchronously();
        if(ready())return;
        juce::Thread::sleep(5);
    }
    check(false,"editor did not refresh its configured scale through GUI timers");
}
void parametersAndState(){
    FlysnapperProcessor a,b;
    check(a.acceptsMidi()&&a.producesMidi()&&a.isMidiEffect(),"MIDI-only role declarations");
    check(a.getTotalNumInputChannels()==0&&a.getTotalNumOutputChannels()==0,"MIDI-only processor advertises audio channels");
    check(a.getBusCount(true)==0&&a.getBusCount(false)==0,"MIDI-only processor advertises audio buses");
    check(a.getLatencySamples()==0,"MIDI correction reports intentional latency");
    for(const char* old:{"pitchEnabled","pitchMode","monoBend","bendRange","autoInput","mpeOutput","inputGate","dynamicsEnabled","gateThresholdDb","compressorRatio"})check(a.parameters.getParameter(old)==nullptr,"removed audio parameter remains exposed");
    const auto defaults=flysnapper::factoryDefaults();
    auto* modes=dynamic_cast<juce::AudioParameterChoice*>(a.parameters.getParameter("mode"));
    check(modes&&modes->choices[0]=="Chord Tones Only"&&modes->choices[1]=="Chord-Scale Assignment","correction mode terminology incorrect");
    for(int i=0;i<50;++i){
        const auto id=FlysnapperProcessor::mappingParameterId(i);check(id==juce::String("map_")+flysnapper::chordTypes()[size_t(i)].id,"mapping ID changed");
        auto* choice=dynamic_cast<juce::AudioParameterChoice*>(a.parameters.getParameter(id));check(choice&&choice->choices.size()==flysnapper::scaleCount,"mapping must expose all31 scales");check(get(a,id)==float(defaults[size_t(i)]),"factory mapping changed without user assignment file");
    }
    for(int scale=0;scale<flysnapper::scaleCount;++scale){
        for(int i=0;i<50;++i)set(a,FlysnapperProcessor::mappingParameterId(i),float(scale));
        set(a,"mode",2);set(a,"root",float(scale%12));set(a,"scale",float(scale));saveLoad(a,b);
        for(int i=0;i<50;++i)check(get(b,FlysnapperProcessor::mappingParameterId(i))==float(scale),"50x31 mapping roundtrip failed");
        check(get(b,"scale")==scale&&get(b,"root")==scale%12&&get(b,"mode")==2,"fixed-scale state roundtrip failed");
    }
    for(int i=0;i<50;++i)set(a,FlysnapperProcessor::mappingParameterId(i),float((i*7+3)%flysnapper::scaleCount));
    set(a,"chordThru",1);set(a,"help",0);saveLoad(a,b);
    check(get(b,"chordThru")==1&&get(b,"help")==0,"MIDI routing/help state lost");
    set(a,FlysnapperProcessor::mappingParameterId(0),0);check(get(b,FlysnapperProcessor::mappingParameterId(0))==3,"instances share assignment storage");
    auto xml=stateXml(a);check(xml!=nullptr,"state XML unavailable");auto* values=xml->getChildByName("FLYSNAPPER_PARAMETERS");check(values!=nullptr,"state parameter tree missing");
    for(auto* child:values->getChildIterator())if(child->getStringAttribute("id").startsWith("map_"))child->setAttribute("value",30);
    loadXml(b,*xml);
    for(int i=0;i<50;++i)check(get(b,FlysnapperProcessor::mappingParameterId(i))==float(i==0?0:(i*7+3)%flysnapper::scaleCount),"stable scale IDs did not override numeric caches");
    auto* table=xml->getChildByName("ChordScales");check(table!=nullptr,"stable chord mapping table missing");table->getFirstChildElement()->setAttribute("scale","unknown_future_scale");loadXml(b,*xml);
    check(get(b,FlysnapperProcessor::mappingParameterId(0))==defaults[0],"unknown scale did not restore row's factory choice");
    // Legacy payload fields are tolerated, but must not restore removed audio controls.
    auto* legacy=values->createNewChildElement("PARAM");legacy->setAttribute("id","pitchEnabled");legacy->setAttribute("value",1);
    legacy=values->createNewChildElement("PARAM");legacy->setAttribute("id","monoBend");legacy->setAttribute("value",1);
    loadXml(b,*xml);check(b.parameters.getParameter("pitchEnabled")==nullptr&&b.parameters.getParameter("monoBend")==nullptr,"legacy state reintroduced audio parameters");
    const auto previous=get(b,"root");xml->setAttribute("schema",999);loadXml(b,*xml);check(get(b,"root")==previous,"unknown schema changed state");
    juce::XmlElement foreign("BUFFKILLER_STATE");loadXml(b,foreign);check(get(b,"root")==previous,"foreign product state accepted");
    const char invalid[]{'b','a','d','\0'};b.setStateInformation(invalid,4);check(get(b,"root")==previous,"invalid state changed parameters");
    set(a,"routingTest",1);saveLoad(a,b);check(get(b,"routingTest")==0,"state load armed transient routing pulse");
    a.prepareToPlay(48000,128);set(a,"correction",0);process(a,128,{{0,on(60)}});saveLoad(a,b);b.prepareToPlay(48000,128);check(process(b,128).empty(),"state load restored active playing notes");
    std::cout<<"PASS MIDI-only parameters,50x31 state choices, stable IDs, independent copies and legacy XML payloads\n";
}
void realtimeNewCheck(){
    FlysnapperProcessor p;p.prepareToPlay(48000,128);set(p,"mode",2);set(p,"scale",1);
    juce::AudioBuffer<float> audio(0,128);juce::MidiBuffer midi;midi.ensureSize(131072);
    for(int i=0;i<8;++i){midi.clear();midi.addEvent(on(61),7);midi.addEvent(off(61),71);p.processBlock(audio,midi);}
    newCalls=0;countNew=true;
    for(int i=0;i<128;++i){midi.clear();midi.addEvent(on(61),7);midi.addEvent(off(61),71);p.processBlock(audio,midi);}
    countNew=false;check(newCalls==0,"warmed MIDI processor allocated C++ heap objects");
    std::cout<<"PASS warmed processBlock C++ new check (host wrapper and malloc/realloc excluded)\n";
}
void guiBeforeProcessing(){
    FlysnapperProcessor p;set(p,"mode",2);set(p,"root",2);set(p,"scale",30);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* active=dynamic_cast<juce::Label*>(find(*editor,"activeScale"));
    auto* tones=dynamic_cast<juce::Label*>(find(*editor,"fixedScaleTones"));
    auto* chord=dynamic_cast<juce::Label*>(find(*editor,"chordDisplay"));
    check(chord&&chord->getText()=="No chord input","new editor does not clearly show absence of chord input");
    check(active&&active->getText().contains("Root D")&&active->getText().contains(juce::String::fromUTF8(flysnapper::scaleDefinitions()[30].name)),"stopped/new editor shows stale processed scale instead of current controls");
    check(tones&&tones->getText().contains("D"),"stopped/new editor does not preview transposed tone names");
    set(p,"scale",1);check(active->getText().contains("Ionian"),"stopped scale edit does not refresh configured scale");
    editor.reset();set(p,"correction",0);editor.reset(p.createEditor());active=dynamic_cast<juce::Label*>(find(*editor,"activeScale"));
    check(active&&active->getText()=="Note Correction off","new editor does not reflect correction-off before processing");
    std::cout<<"PASS scale/root/correction preview before prepareToPlay or processBlock\n";
}
void gui(){
    FlysnapperProcessor p;p.prepareToPlay(48000,128);set(p,"mode",1);process(p,128,cChord());
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());check(editor!=nullptr,"editor creation failed");
    auto* chord=dynamic_cast<juce::Label*>(find(*editor,"chordDisplay"));check(chord&&chord->getText().contains("C"),"chord display absent");check(chord->getFont().getHeight()>=32&&chord->getHeight()>=60,"chord display too small");
    for(const char* old:{"pitchEnabled","pitchMode","monoBend","bendRange","autoInput","mpeOutput","inputGate","sensitivity","releaseMs","inputCalibration","inputMeter","dynamicsTab","trackingTab","dynamicsEnabled"})check(find(*editor,old)==nullptr,"removed audio UI still present");
    auto* mode=dynamic_cast<juce::ComboBox*>(find(*editor,"correctionMode"));auto* fixed=dynamic_cast<juce::ComboBox*>(find(*editor,"fixedScale"));
    auto* thru=dynamic_cast<juce::ToggleButton*>(find(*editor,"chordThru"));auto* correction=dynamic_cast<juce::ToggleButton*>(find(*editor,"correctionEnabled"));
    check(mode&&fixed&&thru&&correction,"MIDI controls absent");check(mode->getItemText(0)=="Chord Tones Only"&&mode->getItemText(1)=="Chord-Scale Assignment","visible mode terminology wrong");
    thru->setToggleState(true,juce::sendNotificationSync);check(get(p,"chordThru")==1,"Chord MIDI Through control is not attached");thru->setToggleState(false,juce::sendNotificationSync);
    set(p,"mode",2);editor.reset();editor.reset(p.createEditor());fixed=dynamic_cast<juce::ComboBox*>(find(*editor,"fixedScale"));check(fixed&&fixed->isEnabled(),"Fixed Scale menu not enabled in fixed mode");
    check(fixed->getNumItems()==flysnapper::scaleCount,"fixed scale menu missing choices");
    const auto fixedOrder=flysnapper::orderedScaleIndices();
    for(int visible=0;visible<flysnapper::scaleCount;++visible)check(fixed->getItemId(visible)==fixedOrder[size_t(visible)]+1,"fixed menu family/mode order wrong");
    for(int index=0;index<flysnapper::scaleCount;++index){fixed->setSelectedId(index+1,juce::sendNotificationSync);check(get(p,"scale")==index,"fixed visible-order/index mixup");set(p,"scale",float((index+7)%flysnapper::scaleCount));check(fixed->getSelectedId()==(index+7)%flysnapper::scaleCount+1,"fixed host automation selected wrong scale ID");}
    bool sawIncompatible=false;bool sawReordered=false;
    for(int type=0;type<flysnapper::chordTypeCount;++type){
        const auto& chordType=flysnapper::chordTypes()[size_t(type)];const auto id=juce::String(chordType.id);
        auto* box=dynamic_cast<juce::ComboBox*>(find(*editor,"mapping_"+id));auto* name=dynamic_cast<juce::Label*>(find(*editor,"chordName_"+id));
        auto* chordNotes=dynamic_cast<juce::Label*>(find(*editor,"chordTones_"+id));auto* scaleNotes=dynamic_cast<juce::Label*>(find(*editor,"scaleTones_"+id));
        check(box&&name&&chordNotes&&scaleNotes,"mapping row lacks chord/scale tone names");check(!chordNotes->isEditable()&&!scaleNotes->isEditable(),"tone explanation became editable");
        check(chordNotes->getText().contains("C"),"chord formula is not shown relative to C");check(box->getNumItems()==flysnapper::scaleCount,"mapping does not offer all31 scales");
        const auto mask=flysnapper::chordToneMask(type);
        const auto order=flysnapper::orderedScaleIndices(mask);
        bool topHeader=false,bottomHeader=false;
        for(juce::PopupMenu::MenuItemIterator it(*box->getRootMenu());it.next();){const auto& item=it.getItem();if(item.isSectionHeader){topHeader|=item.text=="Scales with all chord tones";bottomHeader|=item.text=="Chord tones outside scale";}}
        check(topHeader&&bottomHeader,"mapping menu is missing compatibility section headings");
        for(int visible=0;visible<flysnapper::scaleCount;++visible){
            const int index=order[size_t(visible)];check(box->getItemId(visible)==index+1,"mapping menu lost chord compatibility/family order");check(box->isItemEnabled(index+1),"incompatible scale was disabled");
            sawReordered|=visible!=index;
            if(!flysnapper::scaleContainsChord(index,mask)){sawIncompatible=true;check(box->getItemText(visible).startsWith("! "),"incompatible scale lacks visible warning mark");}
        }
        const auto parameterId=FlysnapperProcessor::mappingParameterId(type);
        // The changing visible order must never become the stored choice value.
        for(int index=0;index<flysnapper::scaleCount;++index){
            box->setSelectedId(index+1,juce::sendNotificationSync);check(get(p,parameterId)==index,"mapping selected visible index rather than stable scale ID");
            check(scaleNotes->getText().startsWith("Scale tones: "),"selected scale notes not refreshed");
            if(!flysnapper::scaleContainsChord(index,mask))check(scaleNotes->getText().contains("Outside scale:"),"missing chord tones not named");
            const int automated=(index+11)%flysnapper::scaleCount;set(p,parameterId,float(automated));check(box->getSelectedId()==automated+1,"host automation selected wrong mapping item");
        }
    }
    check(sawIncompatible&&sawReordered,"GUI test failed to exercise incompatible/reordered choices");
    auto* firstName=find(*editor,"chordName_major");check(firstName!=nullptr,"first chord-name column missing");const int chordWidth=firstName->getWidth();
    editor->setSize(1500,1100);check(firstName->getWidth()==chordWidth,"chord-name column expands with window width");editor->setSize(1040,760);
    auto* description=dynamic_cast<juce::Label*>(find(*editor,"backDescription"));auto* tip=dynamic_cast<juce::Label*>(find(*editor,"assignmentTip"));
    check(description&&tip&&description->getFont().isBold()&&!tip->getFont().isBold(),"back description/tip weight mismatch");
    check(description->getText()==juce::String::fromUTF8("50 chord types, here assigned to root C. Your chosen scale type applies in the actual chord\xe2\x80\x99s root and is auto-saved with this instance."),"back description does not match requested text");
    check(tip->getText().contains("Note Correction")&&tip->getText().contains("When routing chord MIDI directly"),"assignment tip omits conditional routing/correction guidance");
    auto* flip=dynamic_cast<juce::TextButton*>(find(*editor,"flip"));auto* viewport=dynamic_cast<juce::Viewport*>(find(*editor,"mappingViewport"));auto* reset=dynamic_cast<juce::TextButton*>(find(*editor,"resetMappings"));
    check(flip&&viewport&&reset,"front/back controls absent");check(!viewport->isVisible(),"mapping view should start hidden");
    reset->onClick();const auto defaults=flysnapper::factoryDefaults();for(int type=0;type<50;++type)check(get(p,FlysnapperProcessor::mappingParameterId(type))==defaults[size_t(type)],"reset did not restore instance factory assignments");
    // A controlled reference remains stable when the user later edits factory
    // choices: C major assigned explicitly to Ionian must correct Db upward.
    const auto major=flysnapper::chordIndexFromId("major"),ionian=flysnapper::scaleIndexFromId("ionian");
    set(p,FlysnapperProcessor::mappingParameterId(major),float(ionian));
    set(p,"mode",1);p.requestPanic();process(p,128);process(p,128,cChord());const auto before=p.getSnapshot();
    check(before.typeIndex==major&&before.effectiveScale==ionian&&before.allowedMask==0xab5,"controlled C-major/Ionian case has incorrect recognition or allowed notes");
    const auto held=process(p,128,{{0,on(61)}});check(held.size()==1&&held[0].second.isNoteOn()&&held[0].second.getNoteNumber()==62,"C-major/Ionian GUI continuity note was not corrected upward to D");
    auto* activeScale=dynamic_cast<juce::Label*>(find(*editor,"activeScale"));check(activeScale!=nullptr,"active scale display missing before capture");
    settleGuiTimers([&]{return activeScale->getText().contains("Ionian")&&activeScale->getText().contains("Root C");});
    check(activeScale->getText().contains("Ionian"),"front capture does not show the controlled Ionian assignment");
    auto* help=dynamic_cast<juce::ToggleButton*>(find(*editor,"help"));auto* guide=dynamic_cast<juce::TextButton*>(find(*editor,"guide"));auto* guideClose=dynamic_cast<juce::TextButton*>(find(*editor,"guideClose"));
    auto* panel=find(*editor,"guidePanel");auto* guideText=dynamic_cast<juce::TextEditor*>(find(*editor,"guideText"));auto* box=dynamic_cast<juce::ComboBox*>(find(*editor,"mapping_major"));
    check(help&&guide&&guideClose&&panel&&guideText&&box,"help/guide controls absent");check(box->getTooltip().isNotEmpty(),"mapping tooltip missing");
    help->setToggleState(false,juce::sendNotificationSync);help->onClick();check(get(p,"help")==0&&box->getTooltip().isEmpty(),"help off did not clear mapping tooltip");
    help->setToggleState(true,juce::sendNotificationSync);help->onClick();check(get(p,"help")==1&&box->getTooltip().isNotEmpty(),"help on did not restore mapping tooltip");
    guide->onClick();check(panel->isVisible()&&guideText->isReadOnly(),"guide failed to open read-only");
    check(guideText->getText().contains("Chord MIDI Through")&&guideText->getText().contains("Device Activator")&&guideText->getText().contains("31 scales"),"guide lacks MIDI-only routing/scale guidance");
    check(!guideText->getText().containsIgnoreCase("hysteresis")&&!guideText->getText().containsIgnoreCase("compressor"),"old audio guide remained");
    check(process(p,128).empty()&&p.getSnapshot().allowedMask==before.allowedMask,"opening guide changed MIDI/allowed notes");screenshot(*editor,"Flysnapper-guide.png");guideClose->onClick();check(!panel->isVisible(),"guide failed to close");
    flip->onClick();check(viewport->isVisible(),"flip failed to show mapping rows");check(process(p,128).empty(),"flip interrupted a held note");screenshot(*editor,"Flysnapper-back.png");
    viewport->setViewPosition(0,49*FlysnapperMappingTable::rowHeight);screenshot(*editor,"Flysnapper-back-last.png");viewport->setViewPosition(0,0);
    flip->onClick();check(!viewport->isVisible(),"flip failed to return");screenshot(*editor,"Flysnapper-front.png");
    check(process(p,128).empty(),"return flip interrupted held note");const auto mask=p.getSnapshot().allowedMask;editor.reset();check(process(p,128).empty()&&p.getSnapshot().allowedMask==mask,"editor close changed active correction");
    const auto released=process(p,128,{{19,off(61)}});check(released.size()==1&&released[0].second.isNoteOff()&&released[0].second.getNoteNumber()==held[0].second.getNoteNumber(),"GUI lifecycle lost playing-note binding");
    editor.reset(p.createEditor());editor->setSize(1210,900);editor.reset();editor.reset(p.createEditor());
    check(editor->getWidth()==1040&&editor->getHeight()==760,"new editor default size differs after closing a resized view");
    std::cout<<"PASS actual MIDI-only editor,50x31 ID-safe menu selections/automation, grouped warnings, note names, fixed column, flip/help continuity and PNGs\n";
}
}
int main(){
    try{juce::ScopedJuceInitialiser_GUI runtime;parametersAndState();realtimeNewCheck();guiBeforeProcessing();gui();std::cout<<"FlysnapperPluginSmoke: PASS ("<<assertions<<" assertions; not Ableton validation)\n";return 0;}
    catch(const std::exception& error){countNew=false;std::cerr<<"FlysnapperPluginSmoke: FAIL: "<<error.what()<<'\n';return 1;}
}
