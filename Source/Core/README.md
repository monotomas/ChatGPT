# MIDI core

This implementation follows the user-approved combined-product behavior.
Earlier routing gates and interim testing requirements in the original handover
are superseded by docs/PRODUCT_SPEC.md.

`MusicData` carries 38 scales and 52 independent chord types. The original
50 recognition patterns remain unchanged; two user-requested #11 types append
to the catalog without remapping existing IDs.
The copied `Reference` headers are AGPL-3.0-or-later Buffkiller source from the
unaltered handover. They provide GUI naming/spelling; the realtime lookup is a
numeric, precomputed equivalent. Exhaustive tests compare all 4096 masks and all
valid bass classes with the reference's retained candidates and first candidate.
The GUI may show alternatives. Chord Scale uses the first candidate:

1. Prefer fewer explicit omissions, then no added ninth.
2. Within that completeness class, prefer root bass, then third, then fifth.
3. Remaining ties preserve the audited dictionary's type order and root order.

An absent root is never invented. Both chord-dependent modes use Chromatic
(mask 0xfff, effective scale/root 0/0) when no chord is recognized: empty, single
pitch class or unrecognized multi-note set. Recognized Chord Notes uses the
actual held pitch classes; Chord Scale uses its assigned scale. There is no
hidden union between that scale and chord tones. A single pitch class displays
as `G (single note)`; a major triad as `G Major`. These are GUI names, not
recognition or registry changes.

## Note and controller policies

- Hold existing output mappings over chord, scale, table, correction bypass and
  mode changes. New Note Ons use the current set; Note Offs use their binding.
- Empty/unrecognized chord sets pass playing pitches chromatically in both
  chord modes. Existing bindings survive fallback transitions. Explicit stopped
  transport overrides all three modes to Chromatic in the adapter without
  changing saved parameters; absent/invalid transport information is unknown.
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

## Factory assignments and presentation

All 52 starting assignments are defined by the preserved user workbook and two
later Chromatic overrides, recorded in
[the final factory record](../../docs/factory-settings/2026-11-07-user-values-final/README.md).
Every row independently permits all 38 scales. Menus group compatible scales
first while retaining stable IDs, and identify chord tones outside other
choices. Scale compatibility is information, not a restriction.

The assignment table presents four columns of 14/12/12/14 types. Presentation
order is separate from the immutable registry indices. C5 appears just after
Cm; the dominant group splits between C7b13 and C7sus4. All columns fit the
minimum editor width. No layout choice changes parameters, defaults or state.
