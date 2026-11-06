# Guitar reliability after the rejected 0.3.0 trial

The user reports four musical defects in the delivered 0.3.0: repeated
triggers, omitted notes, unplayed lower octaves and excessive response time.
The delivered source `e235c8a098dadce6e0a4b8bd6ca9de789a5d174d` remains the
unchanged baseline. Program/installer success is not acoustic acceptance.
Polyphonic bending remains out of scope; mono bending and genuine discrete
articulation remain required. Original data, labels and scoring stay intact.

## Actual processor reproduction

`native-baseline/` retains a fresh execution of the real processor after adding
the six-open-string strum regression to its existing test group. Runtime
`Source/` bytes still match the delivered commit. The selected before/after
compile inventories match; the exact new test source is retained separately.
The first 86 bounded cases pass, but case 87 **fails**:

* Six sources are MIDI40/45/50/55/59/64, starting 7ms apart from 80ms, with
  2ms attacks and independently decaying eight-partial waves. No noise or
  later excitation is supplied.
* Three notes (45/59/64) are omitted. MIDI40 triggers twice; unplayed MIDI43
  is also emitted. Its octave MIDI55 really is present in the source.
* First correct decisions take 87.98/84.65/82.98ms for the three found notes,
  measured from each source's actual start. A missed note has no latency value.

This reproduces the direct-detector failure through sidechain, dynamics,
detector and actual MIDI output. It is a failing regression, not an expected
pass or a general physical-guitar/interface measurement. Existing 0.3.0
native/Windows successes remain historical evidence of their original tests.
The new regression must not be disabled or changed into a passing result to
prepare another installer.

## Independent real plucks

A new [fixture edition](../../../Tests/AudioFixtures/guitar_reliability_2026_11_06/README.md)
retains original licensed electric-guitar recordings. Its seven development
and seven reserved files were selected before any detector output. Reserved
detector runs remain withheld; annotation metadata is visible. These are
isolated samples from one instrument, not continuous human articulation or
an independent unseen-guitar test. Acoustic activity thresholds and file ends
are not physical pick/damping ground truth.

Research candidates and source-bound negative results will be retained
separately. Reducing extras by losing real repicks, or improving timing by
introducing octave errors, is not an accepted solution.

## Bounded nearby-fret correction

The adopted change preserves the full-frame mono estimator for fresh notes,
steady sources and distant/octave changes. A conflicting nearby semitone
hypothesis can use three recent periods, with independently observed root and
upper partials, rather than a frame longer than the entire fast fret plateau.
Initial-note latency is unchanged.

The [window investigation](mono-window-investigation/README.md) retains the
rejected short-fresh/global-window/fallback alternatives and exact paired
captures. The bounded candidate restores 12 omitted plain fret notes, retains
all 74 non-bend references and leaves legacy107 OFF/DEFAULT, real DEV56,
plain recorded-noise and all bend streams unchanged. Existing default-dynamics
missed repicks remain, so this is not a complete musical fix.

[Actual processor integration](native-local-frets/reproduction.json) finds all
eight notes in each newly generated 35ms ascent/descent, around 17–20ms after
each fret change. Its first 88 cases pass; the unchanged poly reproduction
still fails. The [complete native run](full-native-local-frets/result.json)
likewise passes 14/15 groups and fails that same poly case. All 49 selected
project inputs match the fresh compile inventory; this is not a complete
compiler/JUCE/system-header dependency closure. No installer is produced.

The [portable real-pluck benchmark](real-pluck-dev/README.md) keeps nominal
MIDI-anchor counts separate from effective continuous pitch. Its reserved
recordings remain detector-unrun. The [spectral-template alternative](spectral-template-negative/README.md)
is rejected: lower false-trigger counts concealed additional omissions,
late overlapping-source acquisition, release errors and callback overruns.
