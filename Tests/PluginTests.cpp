// SPDX-License-Identifier: AGPL-3.0-or-later
// Flysnapper processor/editor integration tests. Windows execution is not
// inferred from running this executable on another operating system.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

namespace {
thread_local bool countNew=false;
thread_local unsigned newCalls=0;
void* allocate(size_t size) {
    if(countNew)++newCalls;
    if(auto* ptr=std::malloc(size?size:1))return ptr;
    throw std::bad_alloc();
}
}
void* operator new(size_t n){return allocate(n);}
void* operator new[](size_t n){return allocate(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}
void operator delete[](void* p,size_t)noexcept{std::free(p);}

namespace {
unsigned assertions=0;
void check(bool condition,const char* message){++assertions;if(!condition)throw std::runtime_error(message);}
void set(FlysnapperProcessor& p,const juce::String& id,float value){
    auto* parameter=p.parameters.getParameter(id);check(parameter!=nullptr,"missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(FlysnapperProcessor& p,const juce::String& id){
    auto* value=p.parameters.getRawParameterValue(id);check(value!=nullptr,"missing raw parameter");return value->load();
}
using Midi=std::vector<std::pair<int,juce::MidiMessage>>;
Midi process(FlysnapperProcessor& p,int samples,const Midi& incoming={},bool bypass=false){
    juce::AudioBuffer<float> audio(2,samples);juce::MidiBuffer midi;
    for(int c=0;c<2;++c)for(int n=0;n<samples;++n)audio.setSample(c,n,float((n%11+1)*.031));
    for(const auto& event:incoming)midi.addEvent(event.second,event.first);
    if(bypass)p.processBlockBypassed(audio,midi);else p.processBlock(audio,midi);
    for(int c=0;c<2;++c)for(int n=0;n<samples;++n)check(audio.getSample(c,n)==0.0f,"instrument output is not silent");
    Midi out;for(const auto event:midi)out.emplace_back(event.samplePosition,event.getMessage());return out;
}
juce::MidiMessage on(int note,int channel=1,int velocity=100){return juce::MidiMessage::noteOn(channel,note,juce::uint8(velocity));}
juce::MidiMessage off(int note,int channel=1){return juce::MidiMessage::noteOff(channel,note);}
Midi cChord(int offset=0){return {{offset,on(48,16)},{offset,on(52,16)},{offset,on(55,16)}};}
void expectNote(const Midi& events,bool noteOn,int note,int offset,int channel=1){
    check(events.size()==1,"unexpected MIDI event count");
    const auto& e=events.front();check(e.first==offset,"sample offset changed");
    check(noteOn?e.second.isNoteOn():e.second.isNoteOff(),"wrong note event kind");
    check(e.second.getNoteNumber()==note&&e.second.getChannel()==channel,"wrong note/channel output binding");
}
void saveLoad(FlysnapperProcessor& from,FlysnapperProcessor& to){
    juce::MemoryBlock state;from.getStateInformation(state);check(state.getSize()>0,"state empty");
    to.setStateInformation(state.getData(),int(state.getSize()));
}
void loadXml(FlysnapperProcessor& p,const juce::XmlElement& xml){
    juce::MemoryBlock state;juce::AudioProcessor::copyXmlToBinary(xml,state);
    p.setStateInformation(state.getData(),int(state.getSize()));
}
std::unique_ptr<juce::XmlElement> stateXml(FlysnapperProcessor& p){
    juce::MemoryBlock state;p.getStateInformation(state);
    return juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));
}
juce::Component* find(juce::Component& parent,const juce::String& id){
    if(parent.getComponentID()==id)return &parent;
    for(int i=0;i<parent.getNumChildComponents();++i)if(auto* result=find(*parent.getChildComponent(i),id))return result;
    return nullptr;
}
class Position final: public juce::AudioPlayHead {
public:
    int64_t samples=0;bool playing=true;
    juce::Optional<PositionInfo> getPosition()const override {
        PositionInfo p;p.setIsPlaying(playing);p.setTimeInSamples(samples);return p;
    }
};
void parametersAndState(){
    FlysnapperProcessor a,b;
    check(a.getParameters().size()==64,"expected fourteen controls plus 50 mappings");
    check(get(a,"monoBend")==0&&get(a,"bendRange")==0,"mono bend must default off with two-semitone range");
    check(a.acceptsMidi()&&a.producesMidi()&&!a.isMidiEffect()&&!a.supportsMPE(),"plugin role declarations");
    check(a.getLatencySamples()==0,"MIDI correction unexpectedly reports delay");
    const auto defaults=flysnapper::factoryDefaults();
    for(int i=0;i<50;++i){
        auto id=FlysnapperProcessor::mappingParameterId(i);
        check(id==juce::String("map_")+flysnapper::chordTypes()[size_t(i)].id,"mapping uses unstable ID");
        auto* choice=dynamic_cast<juce::AudioParameterChoice*>(a.parameters.getParameter(id));
        check(choice!=nullptr&&choice->choices.size()==19,"mapping does not expose all 19 scales");
        check(get(a,id)==float(defaults[size_t(i)]),"wrong factory mapping");
    }
    for(int scale=0;scale<19;++scale){
        for(int i=0;i<50;++i)set(a,FlysnapperProcessor::mappingParameterId(i),float(scale));
        set(a,"mode",2);set(a,"root",float(scale%12));set(a,"scale",float(scale));
        saveLoad(a,b);
        for(int i=0;i<50;++i)check(get(b,FlysnapperProcessor::mappingParameterId(i))==float(scale),"50x19 mapping roundtrip failed");
        check(get(b,"scale")==scale&&get(b,"root")==scale%12&&get(b,"mode")==2,"fixed-scale roundtrip failed");
    }
    for(int i=0;i<50;++i)set(a,FlysnapperProcessor::mappingParameterId(i),float((i*7+3)%19));
    saveLoad(a,b);
    for(int i=0;i<50;++i)check(get(b,FlysnapperProcessor::mappingParameterId(i))==float((i*7+3)%19),"mixed mapping roundtrip failed");
    set(a,FlysnapperProcessor::mappingParameterId(0),0);
    check(get(b,FlysnapperProcessor::mappingParameterId(0))==3,"instances share mapping state");
    auto xml=stateXml(a);check(xml!=nullptr,"state XML unavailable");
    auto* values=xml->getChildByName("FLYSNAPPER_PARAMETERS");check(values!=nullptr,"state parameter tree missing");
    // Corrupt numeric row positions: the string type/scale IDs remain authoritative.
    for(auto* child:values->getChildIterator())if(child->getStringAttribute("id").startsWith("map_"))child->setAttribute("value",18);
    loadXml(b,*xml);
    for(int i=0;i<50;++i)check(get(b,FlysnapperProcessor::mappingParameterId(i))==float(i==0?0:(i*7+3)%19),"stable scale IDs did not override numeric cache");
    auto* table=xml->getChildByName("ChordScales");check(table!=nullptr,"stable mapping table missing");
    table->getFirstChildElement()->setAttribute("scale","unknown_future_scale");
    loadXml(b,*xml);check(get(b,FlysnapperProcessor::mappingParameterId(0))==defaults[0],"unknown scale should retain factory default");
    const auto previous=get(b,"root");
    set(a,"monoBend",1);set(a,"bendRange",1);saveLoad(a,b);
    check(get(b,"monoBend")==1&&get(b,"bendRange")==1,"mono bend settings lost across state restore");
    auto legacy=stateXml(a);
    auto* legacyValues=legacy->getChildByName("FLYSNAPPER_PARAMETERS");
    check(legacyValues!=nullptr,"legacy compatibility state missing");
    for(auto* child=legacyValues->getFirstChildElement();child!=nullptr;){
        auto* next=child->getNextElement();
        const auto id=child->getStringAttribute("id");
        if(id=="monoBend"||id=="bendRange")legacyValues->removeChildElement(child,true);
        child=next;
    }
    loadXml(b,*legacy);
    check(get(b,"monoBend")==0&&get(b,"bendRange")==0,"legacy preset retained previous instance bend settings");
    xml->setAttribute("schema",999);loadXml(b,*xml);check(get(b,"root")==previous,"unknown schema changed state");
    juce::XmlElement foreign("BUFFKILLER_STATE");loadXml(b,foreign);check(get(b,"root")==previous,"foreign product state accepted");
    const char invalid[]{'b','a','d','\0'};b.setStateInformation(invalid,4);check(get(b,"root")==previous,"invalid binary state changed parameters");
    set(a,"routingTest",1);saveLoad(a,b);check(get(b,"routingTest")==0,"test pulse armed by preset restore");
    a.prepareToPlay(48000,128);set(a,"correction",0);process(a,128,{{0,on(60)}});saveLoad(a,b);
    b.prepareToPlay(48000,128);check(process(b,128).empty(),"state restored old live notes");
    std::cout<<"PASS 50 x 19 choices, stable IDs, mixed table, instances, malformed state, transient test reset\n";
}
void routingAndLifecycle(){
    FlysnapperProcessor p;p.prepareToPlay(48000,128);set(p,"mode",0);
    check(process(p,128,cChord()).empty(),"chord control leaked to output");
    expectNote(process(p,128,{{17,on(61,2,87)}}),true,60,17,2);
    auto next=Midi{{0,off(48,16)},{0,off(52,16)},{0,off(55,16)},{0,on(50,16)},{0,on(54,16)},{0,on(57,16)}};
    check(process(p,128,next).empty(),"chord switch changed held note");
    expectNote(process(p,128,{{51,off(61,2)}}),false,60,51,2);
    // Performer event inserted first at the same time still sees the complete new chord.
    p.requestPanic();process(p,128);
    Midi same={{31,on(61)}};auto chord=cChord(31);same.insert(same.end(),chord.begin(),chord.end());
    expectNote(process(p,128,same),true,60,31);
    expectNote(process(p,128,{{83,on(61,1,0)}}),false,60,83);
    p.requestPanic();process(p,128);
    Midi inside={{5,on(61)}};chord=cChord(7);inside.insert(inside.end(),chord.begin(),chord.end());inside.push_back({9,on(61)});
    expectNote(process(p,128,inside),true,60,9);
    check(process(p,128,{{0,off(61)}}).empty(),"suppressed-note FIFO binding lost");
    expectNote(process(p,128,{{0,off(61)}}),false,60,0);
    // Correction bypass preserves old binding, while new notes pass unchanged.
    set(p,"correction",0);expectNote(process(p,128,{{2,on(61)}}),true,61,2);
    set(p,"correction",1);expectNote(process(p,128,{{3,off(61)}}),false,61,3);
    expectNote(process(p,128,{{5,on(63)}},true),true,63,5);
    expectNote(process(p,128,{{6,off(63)}}),false,63,6);
    // Generated and external share correction; stopping generated does not stop an external collision.
    set(p,"mode",2);set(p,"scale",0);
    expectNote(process(p,128,{{0,on(60)}}),true,60,0);
    p.requestTestNote();check(process(p,128).empty(),"colliding generated note retriggered shared output");
    check(process(p,128,{},true).empty(),"stopping generated source terminated external held note");
    expectNote(process(p,128,{{41,off(60)}}),false,60,41);
    p.requestTestNote();expectNote(process(p,128),true,60,0);
    expectNote(process(p,128,{},true),false,60,0);
    // Sustain is handled inside Flysnapper; CC64 is not redundantly sent downstream.
    expectNote(process(p,128,{{0,on(64)}}),true,64,0);
    check(process(p,128,{{4,juce::MidiMessage::controllerEvent(1,64,127)},{10,off(64)}}).empty(),"sustain released held note early");
    expectNote(process(p,128,{{20,juce::MidiMessage::controllerEvent(1,64,0)}}),false,64,20);
    expectNote(process(p,128,{{0,on(67)}}),true,67,0);
    p.requestPanic();auto panic=process(p,128);
    check(std::any_of(panic.begin(),panic.end(),[](const auto& event){return event.second.isNoteOff()&&event.second.getNoteNumber()==67;}),"panic omitted active note-off");
    check(process(p,128).empty(),"panic repeated old lifecycle");
    // Remaining channel messages preserve their channel and sample positions in processor adapter.
    auto messages=Midi{{3,juce::MidiMessage::pitchWheel(3,10000)},{4,juce::MidiMessage::channelPressureChange(4,82)},
                      {5,juce::MidiMessage::aftertouchChange(5,62,71)},{6,juce::MidiMessage::controllerEvent(6,1,55)}};
    const auto forwarded=process(p,128,messages);check(forwarded.size()==messages.size(),"channel expression was discarded");
    for(size_t i=0;i<messages.size();++i){check(forwarded[i].first==messages[i].first,"expression offset changed");
        check(forwarded[i].second.getRawDataSize()==messages[i].second.getRawDataSize()&&
              std::equal(forwarded[i].second.getRawData(),forwarded[i].second.getRawData()+forwarded[i].second.getRawDataSize(),messages[i].second.getRawData()),"expression bytes changed");}
    std::cout<<"PASS block-internal chord timing, routing role isolation, note binding, bypass, generated/external, sustain, panic, expression\n";
}
void timingTransportAndLimits(){
    for(double rate:{44100.,48000.,96000.})for(int size:{1,17,64,257,1024}){
        FlysnapperProcessor p;p.prepareToPlay(rate,size);set(p,"correction",0);
        p.requestTestNote();int64_t timeline=0;int ons=0,offs=0;int64_t offAt=-1;
        const int64_t expected=std::llround(rate*.1);
        while(timeline<expected+size*2){auto output=process(p,size);
            for(const auto& e:output){if(e.second.isNoteOn())++ons;if(e.second.isNoteOff()){++offs;offAt=timeline+e.first;}}
            timeline+=size;
        }
        check(ons==1&&offs==1&&offAt==expected,"generated note duration depends on block or rate");
    }
    FlysnapperProcessor p;p.prepareToPlay(48000,128);set(p,"correction",0);
    Position position;p.setPlayHead(&position);
    expectNote(process(p,128,{{0,on(62)}}),true,62,0);position.samples=128;
    check(process(p,128).empty(),"contiguous transport ended note");position.samples=100000;
    auto seek=process(p,128);check(std::any_of(seek.begin(),seek.end(),[](const auto& e){return e.second.isNoteOff()&&e.second.getNoteNumber()==62;}),"seek failed to release note");
    position.samples+=128;expectNote(process(p,128,{{0,on(64)}}),true,64,0);position.playing=false;
    auto stop=process(p,128);check(std::any_of(stop.begin(),stop.end(),[](const auto& e){return e.second.isNoteOff()&&e.second.getNoteNumber()==64;}),"stop failed to release note");
    p.setPlayHead(nullptr);
    expectNote(process(p,128,{{0,on(65)}}),true,65,0);
    Midi overload;overload.reserve(2050);for(int i=0;i<2050;++i)overload.push_back({0,juce::MidiMessage::controllerEvent(1,1,i%128)});
    const auto output=process(p,128,overload);check(p.droppedEvents()>0,"input overload not diagnosed");
    check(std::any_of(output.begin(),output.end(),[](const auto& e){return e.second.isNoteOff()&&e.second.getNoteNumber()==65;}),"overload left held note");
    expectNote(process(p,128,{{2,on(67)}}),true,67,2);expectNote(process(p,128,{{3,off(67)}}),false,67,3);
    // Reset and event-only callbacks must also end held notes.
    process(p,128,{{0,on(69)}});p.reset();const auto reset=process(p,0);
    check(std::any_of(reset.begin(),reset.end(),[](const auto& e){return e.second.isNoteOff()&&e.second.getNoteNumber()==69;}),"zero-frame reset left note active");
    juce::AudioProcessor::BusesLayout layout=p.getBusesLayout();
    check(layout.inputBuses.size()==1&&layout.outputBuses.size()==1,"instrument bus counts");
    for(auto channels:{juce::AudioChannelSet::disabled(),juce::AudioChannelSet::mono(),juce::AudioChannelSet::stereo()}){
        layout.inputBuses.set(0,channels);check(p.setBusesLayout(layout),"supported sidechain layout rejected");
        process(p,64);check(p.sidechainActive()==!channels.isDisabled(),"sidechain bus state incorrect");
    }
    std::cout<<"PASS generated sample schedule at 3 rates x 5 blocks, seek/stop, zero-frame reset, bounded overload, sidechain bus layouts\n";
}
void realtimeNewCheck(){
    FlysnapperProcessor p;p.prepareToPlay(48000,128);set(p,"mode",2);set(p,"scale",1);
    juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;midi.ensureSize(131072);
    // Caller storage is reserved; private output storage remains owned by the processor.
    for(int i=0;i<8;++i){midi.clear();midi.addEvent(on(61),7);midi.addEvent(off(61),71);p.processBlock(audio,midi);}
    newCalls=0;countNew=true;
    for(int i=0;i<128;++i){midi.clear();midi.addEvent(on(61),7);midi.addEvent(off(61),71);p.processBlock(audio,midi);}
    countNew=false;check(newCalls==0,"processor allocated C++ heap objects during warmed processing");
    std::cout<<"PASS warmed processor C++ new allocation check (malloc/realloc and host wrapper excluded)\n";
}
void gui(){
    FlysnapperProcessor p;p.prepareToPlay(48000,128);set(p,"mode",1);process(p,128,cChord());
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());check(editor!=nullptr,"editor creation failed");
    auto* chord=dynamic_cast<juce::Label*>(find(*editor,"chordDisplay"));check(chord&&chord->getText().contains("C"),"chord display missing");
    check(chord->getFont().getHeight()>=32&&chord->getHeight()>=60,"chord display too small");
    auto* flip=dynamic_cast<juce::TextButton*>(find(*editor,"flip"));check(flip!=nullptr,"flip missing");
    auto* viewport=dynamic_cast<juce::Viewport*>(find(*editor,"mappingViewport"));check(viewport!=nullptr,"mapping viewport missing");
    auto* monoBend=dynamic_cast<juce::ToggleButton*>(find(*editor,"monoBend"));
    auto* bendRange=dynamic_cast<juce::ComboBox*>(find(*editor,"bendRange"));
    auto* bendStatus=dynamic_cast<juce::Label*>(find(*editor,"bendStatus"));
    auto* routingHelp=dynamic_cast<juce::TextButton*>(find(*editor,"routingHelp"));
    check(monoBend&&bendRange&&bendStatus&&routingHelp,"bend/routing controls missing from editor");
    auto* automatic=dynamic_cast<juce::ToggleButton*>(find(*editor,"autoInput"));
    auto* manual=dynamic_cast<juce::Slider*>(find(*editor,"inputGate"));
    auto* mpe=dynamic_cast<juce::ToggleButton*>(find(*editor,"mpeOutput"));
    check(automatic&&manual&&mpe,"automatic input and MPE controls missing from editor");
    check(automatic->getToggleState()&&!manual->isEnabled(),"manual threshold active while automatic input is selected");
    automatic->setToggleState(false,juce::sendNotificationSync);
    mpe->setToggleState(true,juce::sendNotificationSync);
    check(get(p,"autoInput")==0&&get(p,"mpeOutput")==1,"new controls did not update processor settings");
    editor.reset();editor.reset(p.createEditor());
    manual=dynamic_cast<juce::Slider*>(find(*editor,"inputGate"));
    monoBend=dynamic_cast<juce::ToggleButton*>(find(*editor,"monoBend"));
    bendRange=dynamic_cast<juce::ComboBox*>(find(*editor,"bendRange"));
    bendStatus=dynamic_cast<juce::Label*>(find(*editor,"bendStatus"));
    check(manual&&manual->isEnabled(),"manual threshold unavailable after automatic input is disabled");
    check(monoBend&&bendRange&&!monoBend->isEnabled()&&!bendRange->isEnabled(),"legacy bend controls active during MPE output");
    check(bendStatus&&bendStatus->getText().contains("48"),"MPE receiver range not visible");
    set(p,"mpeOutput",0);set(p,"autoInput",1);
    editor.reset();editor.reset(p.createEditor());
    monoBend=dynamic_cast<juce::ToggleButton*>(find(*editor,"monoBend"));
    bendRange=dynamic_cast<juce::ComboBox*>(find(*editor,"bendRange"));
    check(monoBend&&bendRange,"legacy controls missing after MPE mode exit");
    check(bendRange->getNumItems()==2,"bend range must expose two and twelve semitones");
    monoBend->setToggleState(true,juce::sendNotificationSync);
    bendRange->setSelectedItemIndex(1,juce::sendNotificationSync);
    check(get(p,"monoBend")==1&&get(p,"bendRange")==1,"bend UI attachments did not update processor");
    set(p,"pitchMode",1);
    // This console test has no host message loop. Opening the editor must
    // nevertheless reflect the stored mode immediately, without a timer tick.
    editor.reset();editor.reset(p.createEditor());
    monoBend=dynamic_cast<juce::ToggleButton*>(find(*editor,"monoBend"));
    bendRange=dynamic_cast<juce::ComboBox*>(find(*editor,"bendRange"));
    check(monoBend&&bendRange,"reopened bend controls missing");
    check(!monoBend->isEnabled()&&!bendRange->isEnabled(),"mono bend controls remain active in Poly");
    check(get(p,"monoBend")==1&&get(p,"bendRange")==1,"switching to Poly discarded stored mono bend choices");
    set(p,"pitchMode",0);set(p,"monoBend",0);set(p,"bendRange",0);
    editor.reset();editor.reset(p.createEditor());
    monoBend=dynamic_cast<juce::ToggleButton*>(find(*editor,"monoBend"));
    flip=dynamic_cast<juce::TextButton*>(find(*editor,"flip"));
    viewport=dynamic_cast<juce::Viewport*>(find(*editor,"mappingViewport"));
    check(monoBend&&flip&&viewport,"reopened front/back controls missing");
    check(monoBend->isEnabled()&&!monoBend->getToggleState(),"mono bend control failed to restore in Mono");
    check(!viewport->isVisible(),"back side should start hidden");
    for(const auto& type:flysnapper::chordTypes()){
        auto* box=dynamic_cast<juce::ComboBox*>(find(*editor,"mapping_"+juce::String(type.id)));
        check(box&&box->getNumItems()==19,"back side missing 50x19 choice");
        for(int i=0;i<19;++i)check(box->isItemEnabled(i+1),"scale compatibility filtering found");
    }
    const auto before=p.getSnapshot();const auto note=process(p,128,{{0,on(61)}});
    check(!note.empty()&&note.front().second.isNoteOn(),"GUI test note missing");
    flip->onClick();check(viewport->isVisible(),"flip failed to reveal mapping table");
    check(process(p,128).empty(),"flip interrupted held note");
    check(p.getSnapshot().allowedMask==before.allowedMask,"flip changed allowed notes");
    const auto back=editor->createComponentSnapshot(editor->getLocalBounds());
    {
        juce::FileOutputStream file(juce::File::getCurrentWorkingDirectory().getChildFile("Flysnapper-back.png"));
        check(file.openedOk(),"back screenshot file failed");file.setPosition(0);file.truncate();
        check(juce::PNGImageFormat().writeImageToStream(back,file),"back screenshot failed");
    }
    flip->onClick();check(!viewport->isVisible(),"return flip failed");
    const auto front=editor->createComponentSnapshot(editor->getLocalBounds());
    {
        juce::FileOutputStream file(juce::File::getCurrentWorkingDirectory().getChildFile("Flysnapper-front.png"));
        check(file.openedOk(),"front screenshot file failed");file.setPosition(0);file.truncate();
        check(juce::PNGImageFormat().writeImageToStream(front,file),"front screenshot failed");
    }
    check(process(p,128).empty(),"return flip interrupted held note");
    const auto closedMask=p.getSnapshot().allowedMask;editor.reset();
    check(process(p,128).empty()&&p.getSnapshot().allowedMask==closedMask,"editor close changed engine result");
    const auto released=process(p,128,{{19,off(61)}});check(released.size()==1&&released.front().second.isNoteOff(),"GUI lifecycle lost original note binding");
    std::cout<<"PASS actual JUCE editor: large chord, 50x19 menus, flip/close continuity; front/back PNG snapshots written\n";
}
}
int main(){
    try{
        juce::ScopedJuceInitialiser_GUI init;
        parametersAndState();routingAndLifecycle();timingTransportAndLimits();realtimeNewCheck();gui();
        std::cout<<"FlysnapperPluginSmoke: PASS ("<<assertions<<" assertions)\n";return 0;
    }catch(const std::exception& error){countNew=false;std::cerr<<"FlysnapperPluginSmoke: FAIL: "<<error.what()<<"\n";return 1;}
}
