// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "MusicData.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace flysnapper {
enum class Mode { ChordNotes=0, ChordScale=1, FixedScale=2 };
enum class Source { External=0, Generated=1, Diagnostic=2 };
struct Settings {
    bool correctionEnabled=true;
    Mode mode=Mode::ChordNotes;
    int root=0,scale=1;
    std::array<int,chordTypeCount> chordScales=factoryDefaults();
};
enum class EventType { NoteOn, NoteOff, Controller };
struct Event { EventType type=EventType::NoteOff; int channel=1,note=0,value=0,offset=0; };
struct Output {
    std::array<Event,4096> events{};
    size_t size=0; bool overflow=false;
    void clear() noexcept {size=0;overflow=false;}
};
struct Snapshot {
    uint16_t chordMask=0,allowedMask=0;
    int root=-1,typeIndex=-1,candidateCount=0,bass=-1;
    int effectiveScale=-1,effectiveRoot=-1,mode=0;
    bool correctionEnabled=true;
};
uint64_t packSnapshot(const Snapshot&) noexcept;
Snapshot unpackSnapshot(uint64_t) noexcept;
std::string formatChord(const Snapshot&); // GUI ONLY

// Deterministic policies: retain held
// mappings across changes; chord modes pass chromatically if none is recognised;
// same-input FIFO; each playing Note On reattacks a shared output, while
// reference counting retains it until the last binding ends. Diagnostic
// collisions remain nonintrusive. Sustain and raw-note history are separate
// per source/channel; overlapping legato does not erase melodic direction.
class Engine {
public:
    static constexpr size_t voiceCapacity=512;
    Engine();
    void setSettings(const Settings&) noexcept;
    const Settings& settings() const noexcept {return config;}
    void chordNoteOn(int note,int velocity=100) noexcept;
    void chordNoteOff(int note) noexcept;
    void clearChord() noexcept;
    void refreshChord() noexcept;
    void beginTimestamp(int noteOnCount) noexcept;
    void beginTimestamp(Source,int channel,int noteOnCount) noexcept;
    void noteOn(Source,int channel,int note,int velocity,int offset,Output&) noexcept;
    void noteOff(Source,int channel,int note,int velocity,int offset,Output&) noexcept;
    void controller(Source,int channel,int cc,int value,int offset,Output&) noexcept;
    void stopSource(Source,int offset,Output&) noexcept;
    void panic(int offset,Output&) noexcept;
    Snapshot snapshot() const noexcept {return view;}
    size_t activeVoices() const noexcept;
    bool overloaded() const noexcept {return fault;}
    static int nearest(int note,uint16_t allowed,int previous=-1,int beforePrevious=-1) noexcept;
private:
    struct Voice {
        uint64_t serial=0;
        int input=0,output=-1,channel=1;
        Source source=Source::External;
        bool active=false,released=false;
    };
    struct History {
        int previous=-1,beforePrevious=-1;
        bool ambiguous=false;
    };
    Settings config;
    Snapshot view;
    std::array<uint16_t,128> chordHeld{};
    std::array<Voice,voiceCapacity> voices{};
    std::array<std::array<uint16_t,128>,16> outputRefs{};
    std::array<std::array<bool,16>,3> pedal{};
    std::array<std::array<History,16>,3> histories{};
    // Sostenuto/Hold 2 remain forwarded performance expression, but explicit
    // global panic must release their receiver state even after Note Offs have
    // removed every core binding. These flags are output-channel scoped.
    std::array<uint8_t,16> forwardedHolds{};
    uint64_t serial=0;
    bool fault=false;
    void release(Voice&,int velocity,int offset,Output&) noexcept;
    bool emit(const Event&,Output&) noexcept;
    void failSafe(int offset,Output&) noexcept;
    void releaseForwardedHolds(int offset,Output&) noexcept;
    static bool validSource(Source s) noexcept {return int(s)>=0&&int(s)<3;}
    void resetHistory() noexcept {histories={};}
    void updateAllowed() noexcept;
};
}
