// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace buffkiller {
// One atomic word is enough for all 12 unique tone classes in actual bass-to-
// treble order, plus the lowest MIDI pitch. No chord naming on the audio thread.
struct ChordInput {
    std::array<int,12> ordered{};
    int count=0,lowestMidi=-1;
    uint16_t mask=0;
    int bassClass()const noexcept{return lowestMidi<0?-1:lowestMidi%12;}
};
inline uint64_t packChordInput(const ChordInput& in) noexcept {
    uint64_t word=0;
    for(int i=0;i<in.count&&i<12;++i)word|=uint64_t(in.ordered[size_t(i)]&15)<<(4*i);
    word|=uint64_t(in.count&15)<<48;
    if(in.lowestMidi>=0)word|=uint64_t(in.lowestMidi&127)<<52;
    return word;
}
inline ChordInput unpackChordInput(uint64_t word) noexcept {
    ChordInput result;result.count=int((word>>48)&15);
    if(result.count>12)result.count=0;
    for(int i=0;i<result.count;++i){int pc=int((word>>(4*i))&15);if(pc>=12)return {};
        result.ordered[size_t(i)]=pc;result.mask=uint16_t(result.mask|(1u<<pc));}
    result.lowestMidi=result.count?int((word>>52)&127):-1;return result;
}
template<class MidiStateType> inline uint64_t packSoundingNotes(const MidiStateType& state) noexcept {
    ChordInput result;
    for(int n=0;n<128;++n) {
        bool active=false;
        for(int ch=0;ch<16;++ch)if(state.held[size_t(ch)][size_t(n)]||state.sustained[size_t(ch)][size_t(n)]){active=true;break;}
        if(!active)continue;
        if(result.lowestMidi<0)result.lowestMidi=n;
        int pc=n%12;
        if(!(result.mask&(1u<<pc))){result.mask=uint16_t(result.mask|(1u<<pc));result.ordered[size_t(result.count++)]=pc;}
    }
    return packChordInput(result);
}
} // namespace buffkiller
