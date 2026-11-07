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
float get(FlysnapperProcessor& p,const juce::String& id){
    // Choice indices are discrete. APVTS's constructor cache is deliberately
    // unsnapped and may be just below an integer after float normalisation.
    if(auto* choice=dynamic_cast<juce::AudioParameterChoice*>(p.parameters.getParameter(id)))return float(choice->getIndex());
    auto* value=p.parameters.getRawParameterValue(id);check(value!=nullptr,"missing raw parameter");return value->load();
}
using Midi=std::vector<std::pair<int,juce::MidiMessage>>;
Midi process(FlysnapperProcessor& p,int samples,const Midi& input={},bool bypass=false){
    juce::AudioBuffer<float> audio(2,samples);juce::MidiBuffer midi;
    for(int ch=0;ch<2;++ch)for(int s=0;s<samples;++s)audio.setSample(ch,s,.25f);
    for(const auto& e:input)midi.addEvent(e.second,e.first);
    if(bypass)p.processBlockBypassed(audio,midi);else p.processBlock(audio,midi);
    check(audio.getMagnitude(0,samples)==0.0f,"instrument output must be silent");
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
void mappingLayout(juce::AudioProcessorEditor& editor,juce::Viewport& viewport){
    const std::vector<std::vector<const char*>> columns{
        {"major","sus2","sus4","major_sixth","six_nine","major_seventh","major_seventh_sharp5","major_seventh_sharp9","major_seventh_sharp11","major_ninth","major_ninth_sharp11","major_eleventh","major_thirteenth","major_thirteenth_sharp11"},
        {"dominant_seventh","power_seventh","dominant_sharp5","dominant_sharp5_sharp9","dominant_sharp5_flat9","dominant_sharp9","dominant_sharp11","dominant_flat5","dominant_flat5_sharp9","dominant_flat5_flat9","dominant_flat9","dominant_flat13"},
        {"dominant_sus4","dominant_sus4_flat9","dominant_ninth","ninth_sharp11","ninth_sus4","dominant_eleventh","dominant_thirteenth","thirteenth_sharp9","thirteenth_add_sharp9","thirteenth_flat9","thirteenth_sus4","thirteenth_sus4_flat9"},
        {"minor","power","minor_sixth","minor_seventh","half_diminished","minor_ninth","minor_eleventh","minor_eleventh_flat5","minor_thirteenth","minor_thirteenth_flat5","minor_major_seventh","minor_major_ninth","minor_major_thirteenth","diminished_seventh"}
    };
    auto* table=dynamic_cast<FlysnapperMappingTable*>(viewport.getViewedComponent());
    check(table&&table->getHeight()==FlysnapperMappingTable::contentHeight,"mapping table height must cover the longest column");
    check(table->getHeight()<52*86/2,"four-column table did not substantially reduce scrolling");
    check(table->getWidth()<=viewport.getMaximumVisibleWidth()&&viewport.getViewPositionX()==0,"minimum editor requires horizontal scrolling");
    std::array<bool,flysnapper::chordTypeCount> seen{};
    int previousRight=0;
    for(int column=0;column<4;++column){
        const auto bounds=table->columnBounds(column);
        check(bounds.getX()>=previousRight&&bounds.getRight()<=table->getWidth(),"mapping columns overlap or extend beyond the visible width");
        previousRight=bounds.getRight();
        check(int(columns[size_t(column)].size())==FlysnapperMappingTable::columnRows[size_t(column)],"approved column count changed");
        auto* heading=find(editor,"mappingColumn_"+juce::String(column));
        check(heading&&bounds.contains(heading->getBounds()),"mapping column heading is outside its column");
        int lastBottom=FlysnapperMappingTable::headerHeight;
        for(size_t row=0;row<columns[size_t(column)].size();++row){
            const auto id=juce::String(columns[size_t(column)][row]);
            const int type=flysnapper::chordIndexFromId(id.toStdString());
            check(type>=0&&!seen[size_t(type)],"presentation duplicates or loses a stable chord ID");seen[size_t(type)]=true;
            check(FlysnapperMappingTable::typeAt(column,int(row))==type&&FlysnapperMappingTable::columnForType(type)==column&&FlysnapperMappingTable::rowForType(type)==int(row),"approved chord order changed");
            auto* name=find(editor,"chordName_"+id);auto* menu=find(editor,"mapping_"+id);
            auto* tones=find(editor,"chordTones_"+id);auto* scaleTones=find(editor,"scaleTones_"+id);auto* factory=find(editor,"factory_"+id);
            check(name&&menu&&tones&&scaleTones&&factory,"compact cell lost a control or tone/factory explanation");
            for(auto* item:{name,tones,menu,scaleTones,factory})check(bounds.contains(item->getBounds()),"compact cell content clips outside its column");
            check(name->getY()>=lastBottom&&tones->getY()>=name->getBottom()&&menu->getY()>=tones->getBottom()&&scaleTones->getY()>=menu->getBottom()&&factory->getY()>=scaleTones->getBottom(),"compact cell contents overlap");
            check(menu->getX()==name->getX()&&menu->getWidth()==name->getWidth(),"scale menu does not use the full cell width");
            lastBottom=factory->getBottom();
        }
        check(FlysnapperMappingTable::typeAt(column,int(columns[size_t(column)].size()))==-1,"column contains an unapproved extra row");
    }
    check(std::all_of(seen.begin(),seen.end(),[](bool value){return value;}),"four-column presentation omits a chord type");
}
class GuiTimeline final : public juce::AudioPlayHead {
public:
    bool playing=true;int64_t sample=0;
    juce::Optional<PositionInfo> getPosition() const override {PositionInfo p;p.setIsPlaying(playing);p.setTimeInSamples(sample);return p;}
};
void guiFallbacks(){
    for(int mode=0;mode<2;++mode)for(int kind=0;kind<3;++kind){
        FlysnapperProcessor p;p.prepareToPlay(48000,128);set(p,"mode",float(mode));
        Midi input;if(kind==1)input={{0,on(55,16)}};if(kind==2)input={{0,on(48,16)},{0,on(49,16)}};
        process(p,128,input);check(p.getSnapshot().typeIndex<0,"GUI fallback fixture unexpectedly recognized a chord");
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* active=dynamic_cast<juce::Label*>(find(*editor,"activeScale"));
        check(active&&active->getText().contains("Chromatic"),"unknown chord GUI does not show the actual Chromatic fallback");
        if(kind==1){auto* chord=dynamic_cast<juce::Label*>(find(*editor,"chordDisplay"));check(chord&&chord->getText()=="G (single note)","single-note GUI is confused with a major chord");}
        for(int note=0;note<12;++note){auto* chip=dynamic_cast<juce::Label*>(find(*editor,"allowedNote_"+juce::String(note)));check(chip&&chip->findColour(juce::Label::backgroundColourId)==juce::Colour(0xff58d7bb),"unknown-chord preview does not allow all twelve pitch classes");}
    }
    FlysnapperProcessor p;GuiTimeline timeline;p.setPlayHead(&timeline);p.prepareToPlay(48000,128);set(p,"mode",2);set(p,"scale",1);
    process(p,128,cChord());timeline.sample+=128;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* chord=dynamic_cast<juce::Label*>(find(*editor,"chordDisplay"));check(chord&&chord->getText()=="C Major","major triad GUI lacks the Major label");
    auto* active=dynamic_cast<juce::Label*>(find(*editor,"activeScale"));
    timeline.playing=false;process(p,128);timeline.sample+=128;
    settleGuiTimers([&]{return active&&active->getText().contains("Chromatic")&&active->getText().containsIgnoreCase("transport stopped");});
    check(get(p,"mode")==2&&get(p,"scale")==1,"stopped GUI changed saved controls");
    for(int note=0;note<12;++note){auto* chip=dynamic_cast<juce::Label*>(find(*editor,"allowedNote_"+juce::String(note)));check(chip&&chip->findColour(juce::Label::backgroundColourId)==juce::Colour(0xff58d7bb),"stopped GUI fails to show Chromatic allowed notes");}
    p.setPlayHead(nullptr);process(p,128);
    settleGuiTimers([&]{return active->getText().contains("Ionian");});
    check(!p.transportStopped(),"missing playhead retained a stale stopped GUI state");
    std::cout<<"PASS actual GUI single-note/Major labels and Chromatic fallback/transport previews\n";
}
void parametersAndState(){
    FlysnapperProcessor a,b;
    check(flysnapper::chordTypeCount==52&&flysnapper::chordTypes().size()==52,"requested chord catalog must contain all52 types");
    check(flysnapper::scaleCount==38&&flysnapper::scaleDefinitions().size()==38,"requested scale catalog must include all38 scales");
    check(a.acceptsMidi()&&a.producesMidi()&&!a.isMidiEffect(),"instrument adapter MIDI declarations");
    check(a.getTotalNumInputChannels()==0&&a.getTotalNumOutputChannels()==2,"instrument requires zero audio inputs and stereo output");
    check(a.getBusCount(true)==0&&a.getBusCount(false)==1,"instrument audio bus contract");
    check(a.getLatencySamples()==0,"MIDI correction reports intentional latency");
    for(const char* old:{"pitchEnabled","pitchMode","monoBend","bendRange","autoInput","mpeOutput","inputGate","dynamicsEnabled","gateThresholdDb","compressorRatio"})check(a.parameters.getParameter(old)==nullptr,"removed audio parameter remains exposed");
    const auto defaults=flysnapper::factoryDefaults();
    auto* modes=dynamic_cast<juce::AudioParameterChoice*>(a.parameters.getParameter("mode"));
    check(modes&&modes->choices[0]=="Chord Tones Only"&&modes->choices[1]=="Chord-Scale Assignment","correction mode terminology incorrect");
    for(int i=0;i<flysnapper::chordTypeCount;++i){
        const auto id=FlysnapperProcessor::mappingParameterId(i);check(id==juce::String("map_")+flysnapper::chordTypes()[size_t(i)].id,"mapping ID changed");
        auto* choice=dynamic_cast<juce::AudioParameterChoice*>(a.parameters.getParameter(id));check(choice&&choice->choices.size()==flysnapper::scaleCount,"mapping must expose all38 scales");check(get(a,id)==float(defaults[size_t(i)]),"fresh mapping differs from current factory assignment");
    }
    saveLoad(a,b);
    for(int i=0;i<flysnapper::chordTypeCount;++i)check(get(b,FlysnapperProcessor::mappingParameterId(i))==float(defaults[size_t(i)]),"mixed factory assignments did not survive state roundtrip");
    for(int scale=0;scale<flysnapper::scaleCount;++scale){
        for(int i=0;i<flysnapper::chordTypeCount;++i)set(a,FlysnapperProcessor::mappingParameterId(i),float(scale));
        set(a,"mode",2);set(a,"root",float(scale%12));set(a,"scale",float(scale));saveLoad(a,b);
        for(int i=0;i<flysnapper::chordTypeCount;++i)check(get(b,FlysnapperProcessor::mappingParameterId(i))==float(scale),"52x38 mapping roundtrip failed");
        check(get(b,"scale")==scale&&get(b,"root")==scale%12&&get(b,"mode")==2,"fixed-scale state roundtrip failed");
    }
    for(int i=0;i<flysnapper::chordTypeCount;++i)set(a,FlysnapperProcessor::mappingParameterId(i),float((i*7+3)%flysnapper::scaleCount));
    set(a,"chordThru",1);set(a,"help",0);saveLoad(a,b);
    check(get(b,"chordThru")==1&&get(b,"help")==0,"MIDI routing/help state lost");
    set(a,FlysnapperProcessor::mappingParameterId(0),0);check(get(b,FlysnapperProcessor::mappingParameterId(0))==3,"instances share assignment storage");
    auto xml=stateXml(a);check(xml!=nullptr,"state XML unavailable");auto* values=xml->getChildByName("FLYSNAPPER_PARAMETERS");check(values!=nullptr,"state parameter tree missing");
    for(auto* child:values->getChildIterator())if(child->getStringAttribute("id").startsWith("map_"))child->setAttribute("value",flysnapper::scaleCount-1);
    loadXml(b,*xml);
    for(int i=0;i<flysnapper::chordTypeCount;++i)check(get(b,FlysnapperProcessor::mappingParameterId(i))==float(i==0?0:(i*7+3)%flysnapper::scaleCount),"stable scale IDs did not override numeric caches");
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
    std::cout<<"PASS MIDI-only parameters,52x38 state choices, stable IDs, independent copies and legacy XML payloads\n";
}
void realtimeNewCheck(){
    FlysnapperProcessor p;p.prepareToPlay(48000,128);set(p,"mode",2);set(p,"scale",1);
    juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;midi.ensureSize(131072);
    for(int i=0;i<8;++i){midi.clear();midi.addEvent(on(61),7);midi.addEvent(off(61),71);p.processBlock(audio,midi);}
    newCalls=0;countNew=true;
    for(int i=0;i<128;++i){midi.clear();midi.addEvent(on(61),7);midi.addEvent(off(61),71);p.processBlock(audio,midi);}
    countNew=false;check(newCalls==0,"warmed MIDI processor allocated C++ heap objects");
    std::cout<<"PASS warmed processBlock C++ new check (host wrapper and malloc/realloc excluded)\n";
}
void guiBeforeProcessing(){
    FlysnapperProcessor p;set(p,"mode",2);set(p,"root",2);set(p,"scale",float(flysnapper::scaleCount-1));
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* active=dynamic_cast<juce::Label*>(find(*editor,"activeScale"));
    auto* tones=dynamic_cast<juce::Label*>(find(*editor,"fixedScaleTones"));
    auto* chord=dynamic_cast<juce::Label*>(find(*editor,"chordDisplay"));
    check(chord&&chord->getText()=="No chord input","new editor does not clearly show absence of chord input");
    check(active&&active->getText().contains("Root D")&&active->getText().contains(juce::String::fromUTF8(flysnapper::scaleDefinitions().back().name)),"stopped/new editor shows stale processed scale instead of current controls");
    check(tones&&tones->getText().contains("D"),"stopped/new editor does not preview transposed tone names");
    set(p,"scale",1);check(active->getText().contains("Ionian"),"stopped scale edit does not refresh configured scale");
    editor.reset();set(p,"correction",0);editor.reset(p.createEditor());active=dynamic_cast<juce::Label*>(find(*editor,"activeScale"));
    check(active&&active->getText()=="Note Correction off","new editor does not reflect correction-off before processing");
    // Fresh default index3 is one of the constructor values whose raw float
    // cache lies just below its integer with the new 38-choice range.
    editor.reset();
    {
        FlysnapperProcessor fresh;fresh.prepareToPlay(48000,128);set(fresh,"mode",1);
        const int minorMajorNinth=flysnapper::chordIndexFromId("minor_major_ninth");
        check(flysnapper::factoryDefaults()[size_t(minorMajorNinth)]==3,"fresh default3 regression is no longer configured");
        process(fresh,128,{{0,on(48,16)},{0,on(50,16)},{0,on(51,16)},{0,on(55,16)},{0,on(59,16)}});
        check(fresh.getSnapshot().typeIndex==minorMajorNinth&&fresh.getSnapshot().effectiveScale==3,"fresh constructor mapping index3 was truncated before active preview");
        std::unique_ptr<juce::AudioProcessorEditor> freshEditor(fresh.createEditor());
        active=dynamic_cast<juce::Label*>(find(*freshEditor,"activeScale"));
        auto* mapping=dynamic_cast<juce::ComboBox*>(find(*freshEditor,"mapping_minor_major_ninth"));
        check(mapping&&mapping->getSelectedId()==4,"fresh mapping menu truncated constructor choice3");
        auto* factory=dynamic_cast<juce::Label*>(find(*freshEditor,"factory_minor_major_ninth"));
        check(factory&&factory->getText()=="Factory: Harm. Min. 1: Harmonic Minor","fresh CmMaj9 factory label does not match the user assignment");
        check(active&&active->getText().contains("Harm. Min. 1: Harmonic Minor"),"active mapping preview truncated fresh choice3 to a neighbouring scale");
    }
    std::cout<<"PASS scale/root/correction preview before prepareToPlay or processBlock\n";
}
void gui(){
    FlysnapperProcessor p;p.prepareToPlay(48000,128);set(p,"mode",1);process(p,128,cChord());
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());check(editor!=nullptr,"editor creation failed");
    for(const char* id:{"thirteenth_sharp9","thirteenth_add_sharp9"}) {
        auto* choice=dynamic_cast<juce::ComboBox*>(find(*editor,"mapping_"+juce::String(id)));
        auto* tones=dynamic_cast<juce::Label*>(find(*editor,"scaleTones_"+juce::String(id)));
        auto* factory=dynamic_cast<juce::Label*>(find(*editor,"factory_"+juce::String(id)));
        check(choice&&tones&&factory,"retained thirteenth row is missing");
        check(choice->getSelectedId()==1&&get(p,"map_"+juce::String(id))==0,"both thirteenth factories must be Chromatic");
        check(factory->getText()=="Factory: Chromatic"&&!tones->getText().contains("Outside scale:"),"Chromatic factory must use ordinary compatible-scale information");
        check(find(*editor,"factoryExplanation_"+juce::String(id))==nullptr,"canceled exception text remains");
    }
    check(get(p,"map_dominant_sharp9")==13&&get(p,"map_dominant_flat13")==21,"the two workbook corrections are missing");
    auto* chord=dynamic_cast<juce::Label*>(find(*editor,"chordDisplay"));check(chord&&chord->getText().contains("C"),"chord display absent");check(chord->getFont().getHeight()>=32&&chord->getHeight()>=60,"chord display too small");
    for(const char* old:{"pitchEnabled","pitchMode","monoBend","bendRange","autoInput","mpeOutput","inputGate","sensitivity","releaseMs","inputCalibration","inputMeter","dynamicsTab","trackingTab","dynamicsEnabled"})check(find(*editor,old)==nullptr,"removed audio UI still present");
    auto* mode=dynamic_cast<juce::ComboBox*>(find(*editor,"correctionMode"));auto* fixed=dynamic_cast<juce::ComboBox*>(find(*editor,"fixedScale"));
    auto* thru=dynamic_cast<juce::ToggleButton*>(find(*editor,"chordThru"));auto* correction=dynamic_cast<juce::ToggleButton*>(find(*editor,"correctionEnabled"));
    check(mode&&fixed&&thru&&correction,"MIDI controls absent");check(mode->getItemText(0)=="Chord Tones Only"&&mode->getItemText(1)=="Chord-Scale Assignment","visible mode terminology wrong");
    thru->setToggleState(true,juce::sendNotificationSync);check(get(p,"chordThru")==1,"Chord MIDI Through control is not attached");thru->setToggleState(false,juce::sendNotificationSync);
    set(p,"mode",2);editor.reset();editor.reset(p.createEditor());fixed=dynamic_cast<juce::ComboBox*>(find(*editor,"fixedScale"));check(fixed&&fixed->isEnabled(),"Fixed Scale menu not enabled in fixed mode");
    check(fixed->getNumItems()==flysnapper::scaleCount,"fixed scale menu missing choices");
    const auto fixedOrder=flysnapper::orderedScaleIndices();
    juce::StringArray families;
    for(juce::PopupMenu::MenuItemIterator it(*fixed->getRootMenu());it.next();)if(it.getItem().isSectionHeader)families.add(it.getItem().text);
    check(families.size()==11&&families[4]=="Harmonic Minor modes"&&families[5]=="Harmonic Major modes"&&families[6]=="Diminished"&&families[10]=="Minor Blues","Harmonic Major family is missing or incorrectly positioned");
    for(int visible=0;visible<flysnapper::scaleCount;++visible)check(fixed->getItemId(visible)==fixedOrder[size_t(visible)]+1,"fixed menu family/mode order wrong");
    for(int index=0;index<flysnapper::scaleCount;++index){fixed->setSelectedId(index+1,juce::sendNotificationSync);check(get(p,"scale")==index,"fixed visible-order/index mixup");set(p,"scale",float((index+7)%flysnapper::scaleCount));check(fixed->getSelectedId()==(index+7)%flysnapper::scaleCount+1,"fixed host automation selected wrong scale ID");}
    bool sawIncompatible=false;bool sawReordered=false;
    for(int type=0;type<flysnapper::chordTypeCount;++type){
        const auto& chordType=flysnapper::chordTypes()[size_t(type)];const auto id=juce::String(chordType.id);
        auto* box=dynamic_cast<juce::ComboBox*>(find(*editor,"mapping_"+id));auto* name=dynamic_cast<juce::Label*>(find(*editor,"chordName_"+id));
        auto* chordNotes=dynamic_cast<juce::Label*>(find(*editor,"chordTones_"+id));auto* scaleNotes=dynamic_cast<juce::Label*>(find(*editor,"scaleTones_"+id));
        auto* factory=dynamic_cast<juce::Label*>(find(*editor,"factory_"+id));
        check(box&&name&&chordNotes&&scaleNotes&&factory,"mapping row lacks chord/scale tone names or factory label");
        const int initialScale=flysnapper::factoryDefaults()[size_t(type)];
        check(box->getSelectedId()==initialScale+1,"initial mapping selection differs from the user factory assignment");
        check(factory->getText()=="Factory: "+juce::String::fromUTF8(flysnapper::scaleDefinitions()[size_t(initialScale)].name),"displayed factory scale differs from the user factory assignment");check(!chordNotes->isEditable()&&!scaleNotes->isEditable(),"tone explanation became editable");
        check(chordNotes->getText().contains("C"),"chord formula is not shown relative to C");check(box->getNumItems()==flysnapper::scaleCount,"mapping does not offer all38 scales");
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
    // Harmonic Major mode2 in C is C D Eb F Gb A Bb: it contains Cm7b5,
    // but omits the E and G of C major. Both menu choices remain selectable.
    const int harmonicMajor2=flysnapper::scaleIndexFromId("harmonic_major_mode2");
    check(harmonicMajor2==32,"new Harmonic Major mode2 did not retain its appended index");
    auto* halfDim=dynamic_cast<juce::ComboBox*>(find(*editor,"mapping_half_diminished"));
    auto* majorBox=dynamic_cast<juce::ComboBox*>(find(*editor,"mapping_major"));
    auto* majorTones=dynamic_cast<juce::Label*>(find(*editor,"scaleTones_major"));
    check(halfDim&&majorBox&&majorTones,"Harmonic Major compatibility test rows are missing");
    halfDim->setSelectedId(harmonicMajor2+1,juce::sendNotificationSync);
    check(!halfDim->getText().startsWith("! "),"Harmonic Major2 incorrectly marked outside Cm7b5");
    majorBox->setSelectedId(harmonicMajor2+1,juce::sendNotificationSync);
    check(majorBox->getText().startsWith("! ")&&majorTones->getText().endsWith("Outside scale: E  G"),"Harmonic Major2 did not warn about C-major tones E and G");
    auto* firstName=find(*editor,"chordName_major");check(firstName!=nullptr,"first chord-name column missing");const int chordWidth=firstName->getWidth();
    editor->setSize(1500,1100);check(firstName->getWidth()>chordWidth,"four-column cells do not use increased window width");editor->setSize(1040,760);
    auto* description=dynamic_cast<juce::Label*>(find(*editor,"backDescription"));auto* tip=dynamic_cast<juce::Label*>(find(*editor,"assignmentTip"));
    check(description&&tip&&description->getFont().isBold()&&!tip->getFont().isBold(),"back description/tip weight mismatch");
    check(description->getText()==juce::String::fromUTF8("52 chord types, here assigned to root C. Your chosen scale type applies in the actual chord\xe2\x80\x99s root and is auto-saved with this instance."),"back description does not match requested text");
    check(tip->getText().contains("Note Correction")&&tip->getText().contains("MIDI From")
          &&tip->getText().containsIgnoreCase("instrument"),"assignment tip omits instrument routing/correction guidance");
    auto* flip=dynamic_cast<juce::TextButton*>(find(*editor,"flip"));auto* viewport=dynamic_cast<juce::Viewport*>(find(*editor,"mappingViewport"));auto* reset=dynamic_cast<juce::TextButton*>(find(*editor,"resetMappings"));
    check(flip&&viewport&&reset,"front/back controls absent");check(!viewport->isVisible(),"mapping view should start hidden");
    check(viewport->getViewedComponent()&&viewport->getViewedComponent()->getHeight()==FlysnapperMappingTable::contentHeight,"mapping viewport does not include all four columns");
    reset->onClick();const auto defaults=flysnapper::factoryDefaults();for(int type=0;type<flysnapper::chordTypeCount;++type)check(get(p,FlysnapperProcessor::mappingParameterId(type))==defaults[size_t(type)],"reset did not restore instance factory assignments");
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
    check(guideText->getText().contains("Chord MIDI Through")&&guideText->getText().contains("Device Activator")&&guideText->getText().contains("38 scales"),"guide lacks MIDI-only routing/scale guidance");
    check(guideText->getText().containsIgnoreCase("silent stereo")&&guideText->getText().contains("MIDI From"),"guide omits silent instrument output and receiver routing");
    check(guideText->getText().contains("two preceding raw input notes")
          &&guideText->getText().contains("Sequential legato overlap keeps this history")
          &&guideText->getText().contains("Note Off then Note On at the same sample position"),
          "guide omits corrected legato direction and rearticulation behavior");
    check(!guideText->getText().containsIgnoreCase("hysteresis")&&!guideText->getText().containsIgnoreCase("compressor"),"old audio guide remained");
    check(process(p,128).empty()&&p.getSnapshot().allowedMask==before.allowedMask,"opening guide changed MIDI/allowed notes");screenshot(*editor,"Flysnapper-guide.png");guideClose->onClick();check(!panel->isVisible(),"guide failed to close");
    flip->onClick();check(viewport->isVisible(),"flip failed to show mapping rows");check(process(p,128).empty(),"flip interrupted a held note");mappingLayout(*editor,*viewport);screenshot(*editor,"Flysnapper-back.png");
    editor->setSize(1500,1100);mappingLayout(*editor,*viewport);screenshot(*editor,"Flysnapper-back-large.png");editor->setSize(1040,760);
    viewport->setViewPosition(0,FlysnapperMappingTable::contentHeight);screenshot(*editor,"Flysnapper-back-last.png");viewport->setViewPosition(0,0);
    flip->onClick();check(!viewport->isVisible(),"flip failed to return");screenshot(*editor,"Flysnapper-front.png");
    check(process(p,128).empty(),"return flip interrupted held note");const auto mask=p.getSnapshot().allowedMask;editor.reset();check(process(p,128).empty()&&p.getSnapshot().allowedMask==mask,"editor close changed active correction");
    const auto released=process(p,128,{{19,off(61)}});check(released.size()==1&&released[0].second.isNoteOff()&&released[0].second.getNoteNumber()==held[0].second.getNoteNumber(),"GUI lifecycle lost playing-note binding");
    editor.reset(p.createEditor());editor->setSize(1210,900);editor.reset();editor.reset(p.createEditor());
    check(editor->getWidth()==1040&&editor->getHeight()==760,"new editor default size differs after closing a resized view");
    std::cout<<"PASS actual MIDI-only editor,52x38 ID-safe menu selections/automation, grouped warnings, four visible columns14/12/12/14, flip/help continuity and PNGs\n";
}
}
int main(){
    try{juce::ScopedJuceInitialiser_GUI runtime;parametersAndState();realtimeNewCheck();guiBeforeProcessing();guiFallbacks();gui();std::cout<<"FlysnapperPluginSmoke: PASS ("<<assertions<<" assertions; not Ableton validation)\n";return 0;}
    catch(const std::exception& error){countNew=false;std::cerr<<"FlysnapperPluginSmoke: FAIL: "<<error.what()<<'\n';return 1;}
}
