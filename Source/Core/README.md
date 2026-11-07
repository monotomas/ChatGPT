# MIDI core

This implementation follows the user-approved combined-product behavior.
Earlier routing gates and interim testing requirements in the original handover
are superseded by docs/PRODUCT_SPEC.md.

`MusicData` carries the common 19-scale list and the 50 independent chord types.
The copied `Reference` headers are AGPL-3.0-or-later Buffkiller source from the
unaltered handover. They provide GUI naming/spelling; the realtime lookup is a
numeric, precomputed equivalent. Exhaustive tests compare all 4096 masks and all
valid bass classes with the reference's retained candidates and first candidate.
The GUI may show alternatives. Chord Scale uses the first candidate:

1. Prefer fewer explicit omissions, then no added ninth.
2. Within that completeness class, prefer root bass, then third, then fifth.
3. Remaining ties preserve the audited dictionary's type order and root order.

An absent root is never invented. An unknown chord in Chord Scale suppresses new
notes. In Chord Notes the actual received pitch classes remain authoritative,
whether or not they can be named. There is no hidden union between a selected
scale and chord tones.

## Note and controller policies

- Hold existing output mappings over chord, scale, table, correction bypass and
  mode changes. New Note Ons use the current set; Note Offs use their binding.
- An empty chord set suppresses new notes in chord modes, recording a zero-output
  binding so later repeated input notes still get correct FIFO Note Offs.
- Repeated identical input notes match oldest still-held input first (FIFO).
- Playing input voices on the same output channel/note retain separate
  bindings and reference counts. Every new playing attack rearticulates with
  Note Off then Note On at the same offset when the destination is active;
  the last remaining binding releases it. Diagnostic collisions alone keep
  their non-intrusive coalescing behavior. The pair is capacity-checked before
  binding/reference mutation.
- Generated/external sources use the same correction rule. Source termination
  drops only that source's references, preserving another source's shared note.
- CC64 sustain is implemented internally per source/channel. It is consumed;
  corresponding output Note Offs are delayed until pedal release. This avoids
  an external receiver pedal extending notes after source termination. Control
  role CC64 is ignored by the plugin adapter.
- CC120/123 release all voices belonging to that source/channel, ignoring its
  sustain; CC121 resets its sustain and is forwarded. Other CCs are forwarded.
  Channel pressure/bend/etc. are handled separately by the adapter.
- Forwarded CC66 (sostenuto) and CC69 (Hold 2) are remembered per output channel.
  Global panic/reset and capacity fail-safe send their matching pedal-up events,
  even if input Note Offs already removed every core binding. No pedal events
  are added when those controls were not forwarded down. Source-scoped stops
  preserve this shared-channel expression; release the pedal or use global
  Panic when the receiver should also release pedal-held notes immediately.
- Run history uses the two preceding uncorrected inputs in each internal
  source/channel stream, excluding the current note. Ordinary held-note or
  sustain overlap does not erase history. Settings change/panic/failSafe reset
  all streams; source termination and CC120/123 reset only their own scope.
  Multiple positive Note Ons at one timestamp in the same stream (including
  duplicates) invalidate its direction and seed no new history. The adapter
  preclassifies each stream before dispatch. Other sources cannot erase a
  player's history. No lookahead is added; chord updates keep usable history.
- The adapter must batch chord events first at each timestamp, call
  `refreshChord`, then `beginTimestamp(source,channel,count)` for each source
  with the number of its validated positive playing Note Ons at that sample,
  then process performer events. Offsets are carried through unchanged.
- There are 512 input bindings and 4096 output slots. Capacity overflow discards
  pending output, sends CC120/123 on every output channel, clears bindings and
  latches a visible fault. An explicit panic/transport reset enables notes again.
  Capacities do not grow on the audio thread.

Construction prepares the numeric lookup outside processing. Engine operations
do not allocate, lock, build strings or call GUI code. `formatChord`, `chordName`
and `chordDetail` are GUI-only and allocate. Snapshot packs chord recognition,
effective mask, effective scale/root, mode and bypass into a single atomic word.
Active voices and chord input are never preset state.

## Editable factory recommendations

These recommendations are starting values. Every row independently permits all
19 scales, without compatibility filtering. Stable IDs in `MusicData.cpp` are
the persistence keys; row numbers and display strings are not persistence IDs.

| Type | Starting scale |
|---|---|
| major | Major / Ionian |
| m | Natural Minor / Aeolian |
| sus2 | Major / Ionian |
| sus4 | Major / Ionian |
| 5 | Major Pentatonic |
| 5/7 | Mixolydian |
| 6 | Major / Ionian |
| m6 | Dorian |
| 6/9 | Major / Ionian |
| 7 | Mixolydian |
| Maj7 | Major / Ionian |
| m7 | Dorian |
| mMaj7 | Melodic Minor |
| m7b5 | Locrian |
| dim7 | Diminished Whole-Half |
| 7sus4 | Mixolydian |
| 7sus4b9 | Phrygian Dominant |
| 7b5 | Lydian Dominant |
| 7#5 | Altered |
| 7b9 | Phrygian Dominant |
| 7#9 | Altered |
| 7#11 | Lydian Dominant |
| 7b13 | Altered |
| 7b5b9 | Altered |
| 7b5#9 | Altered |
| 7#5b9 | Altered |
| 7#5#9 | Altered |
| Maj7#5 | Melodic Minor |
| Maj7#9 | Harmonic Minor |
| Maj7#11 | Lydian |
| 9 | Mixolydian |
| Maj9 | Major / Ionian |
| m9 | Dorian |
| mMaj9 | Melodic Minor |
| 9sus4 | Mixolydian |
| 9#11 | Lydian Dominant |
| 11 | Mixolydian |
| Maj11 | Major / Ionian |
| m11 | Dorian |
| m11b5 | Locrian |
| 13 | Mixolydian |
| Maj13 | Major / Ionian |
| mMaj13 | Melodic Minor |
| 13b9 | Diminished Half-Whole |
| 13#9 | Diminished Half-Whole |
| 13sus4 | Mixolydian |
| 13sus4b9 | Phrygian Dominant |
| m13 | Dorian |
| 13add#9 | Diminished Half-Whole |
| m13b5 | Locrian |

Unusual altered/extended voicings are one reason this table is editable. A
recommendation need not contain every chord tone and is not a harmonic verdict.
