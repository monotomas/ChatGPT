// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "../Core/MidiEngine.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace flysnapper::midi {
struct Packet { std::array<uint8_t,3> bytes{}; int length=3,offset=0; };
struct Packets {
    // JUCE's VST3 event adapter accepts at most 2048 events per callback.
    static constexpr size_t capacity=2048;
    std::array<Packet,capacity> events{};
    size_t size=0; bool overflow=false;
    void clear() noexcept { size=0;overflow=false; }
};

// MPE conversion. Channel 1 is the lower-zone master; generated
// voices occupy 2..7, external voices 8..15, and chord input 16 never becomes
// an output note. External channel pitch is converted to member expression,
// rather than forwarded to the master and bending every generated string.
// The host must preserve MPE and the receiver must support it. This class
// cannot make an ordinary single-channel instrument bend notes independently.
// In this explicitly selected mode, input 1 is ordinary playing MIDI (default
// wheel range 2); 2..15 are lower-zone MPE members (default range 48), with
// external-only master expression from 1. RPN 6/0 overrides this input layout.
class MpeOutputRouter {
public:
    static constexpr int generatedVoices=6,externalVoices=8,memberRange=48;
    MpeOutputRouter() noexcept;
    void beginBlock(Packets&,int packetBudget=int(Packets::capacity)) noexcept;
    void endBlock(Engine&,Output&,Packets&,int lastOffset) noexcept;
    void observeExternal(Source,int channel,const uint8_t*,int length) noexcept;
    void generatedPitch(int voiceId,int anchor,float pitch,int offset,Packets&) noexcept;
    void generatedOn(Engine&,Output&,int voiceId,int anchor,int velocity,int offset,Packets&) noexcept;
    void generatedOff(Engine&,Output&,int voiceId,int anchor,int velocity,int offset,Packets&) noexcept;
    void external(Engine&,Output&,Source,int channel,const uint8_t*,int length,int offset,Packets&) noexcept;
    // Drain existing core releases before changing output mode or resetting.
    void consumeCore(Output&,Packets&) noexcept;
    // Caller first stops the relevant core source(s), then clears this map.
    // Incoming controller state is retained, so a physically held keyboard
    // wheel/pedal also applies to notes played after a mode/transport change.
    void clear(Packets&,int offset=0) noexcept;
    void clearGenerated(Packets&,int offset=0) noexcept;
    bool faulted() const noexcept { return fault; }
    unsigned stolenVoices() const noexcept { return steals; }
    int activeGenerated() const noexcept;
    int activeExternal() const noexcept;
private:
    static constexpr size_t bindingCapacity=Engine::voiceCapacity;
    struct InputState {
        int wheel=8192,rangeSemitones=2,rangeCents=0,rpnMsb=127,rpnLsb=127,pressure=0,program=-1;
        bool sustain=false,sostenuto=false,hold2=false;
        std::array<int,128> controllers{};
        uint64_t pressureOrder=0,programOrder=0;
        std::array<uint64_t,128> controllerOrder{};
    };
    struct InputZone {int members=14,rangeSemitones=48,rangeCents=0;};
    struct Binding {
        bool used=false,keyDown=false;
        Source source=Source::External;
        int inputChannel=1,inputNote=0,member=0;
        uint64_t serial=0;
    };
    struct Slot {
        bool assigned=false,sounding=false,suppressed=false,latched=false,masterLatched=false;
        Source source=Source::Generated;
        int member=0,anchor=0,outputNote=-1,inputChannel=1,binding=-1,lastWheel=-1;
        uint64_t serial=0;
    };
    struct PendingPitch {bool valid=false;int anchor=0,offset=0;float pitch=0;};
    std::array<Slot,generatedVoices+externalVoices> slots{};
    std::array<PendingPitch,generatedVoices> pending{};
    std::array<Binding,bindingCapacity> bindings{};
    std::array<std::array<InputState,16>,3> input{};
    std::array<InputZone,3> inputZones{};
    uint64_t serial=0,inputSerial=0;
    int softLimit=1984;
    bool zoneConfigured=false,fault=false;
    unsigned steals=0;
    InputState& state(Source source,int channel) noexcept;
    Slot* slotForMember(int member) noexcept;
    void append(Packets&,int status,int a,int b,int length,int offset) noexcept;
    void rpn(Packets&,int channel,int number,int msb,int lsb,int offset) noexcept;
    void ensureZone(Packets&,int offset) noexcept;
    void prepareSlot(Slot&,Packets&,int offset) noexcept;
    void wheel(Slot&,int value,int offset,Packets&,bool force=false) noexcept;
    void releaseSlot(Slot&,Packets&,int offset) noexcept;
    void fail(Engine&,Output&,Packets&,int offset) noexcept;
    void checkBudget(Engine&,Output&,Packets&,int offset) noexcept;
    bool effectivePedal(const Slot&) const noexcept;
    bool inputMember(Source,int channel) const noexcept;
    bool inputTarget(const Slot&,Source,int channel) const noexcept;
    double inputPitch(Source,int channel) const noexcept;
    int inputPressure(Source,int channel) const noexcept;
    int inputController(Source,int channel,int cc) const noexcept;
    int inputProgram(Source,int channel) const noexcept;
    void resetInputControllers(Source,int channel) noexcept;
    void updatePedals(Engine&,Output&,Source,int channel,int offset,Packets&) noexcept;
    void externalOff(Engine&,Output&,Source,int channel,int note,int velocity,int offset,Packets&) noexcept;
    void releaseInputChannel(Engine&,Output&,Source,int channel,int offset,Packets&) noexcept;
};
}
