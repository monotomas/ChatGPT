# Static guitar DSP changes for the November 5 evaluation

This evidence concerns the revised scope: quantized polyphonic notes, retained
monophonic bending, and a version for real-guitar evaluation. It does not claim
that every guitar waveform is transcribed correctly. The previous expression-
enabled `73/216` result used a different polyphonic signal path and scorer;
the fixed-note totals here must not be presented as that earlier readiness score.

## Adopted detector changes

`Settings.allowNewNotes` lets the dynamics gate withhold fresh notes and
same-note rearticulation while existing notes continue through their lower
continuation/release rules. Changing the flag does not reset acquired sources.
Closed-gate background candidates cannot accumulate confirmation or fresh
acquisition history for publication on a later attack. The processor supplies
sample-accurate contiguous admission runs; a future gate opening does not
authorize an earlier decision.

Static polyphonic voices now carry unique bounded IDs `0..5`, allowing the
optional MPE wire format to carry ordinary semitone notes without the old
continuous-poly tracker. Offs preserve the sounding source's ID across reset or
mode changes. This does not identify a physical string across arbitrary fret
changes. No polyphonic pitch-wheel trajectory is required by the revised scope.

The fresh missing-root anchor check now requires a low second or third partial
in addition to the existing fifth/seventh evidence. A directly observed physical
root uses the same rule as before. The change prevents a real C-sharp4 plus an
E4 Hann-window sidelobe from masquerading as the fifth/seventh of A1 and then
replacing both real notes. See the accepted [support audit](missing-root-support/REPORT.md).

The guard removes the false A1 in all six B3-to-C-sharp4 plus held-E4 controls
with `3/4/5/6/8/12` partials. Both intended sources now continue through their
900 ms cutoff rather than ending at about 477 ms. All 396 previously seen
fixed-note event traces and the original 30-case octave grid remain byte-identical.
The native regression retains actual On/Off timestamps and all six partial counts.

**Missing-fundamental limitation:** the guard is deliberately conservative.
On 16 explicit sparse-source cases, correct low-root acquisition falls from
`12/16` to `6/16`. Sources with only fifth/seventh partials, or second/third
partials at 0.2% of the fifth's amplitude, lose six wanted low roots and can
emit upper aliases. At 1% and 5% low-partial support, the measured roots retain
their original exact On/Off times. The lowest test root already fails in both
versions. This is not universal missing-fundamental recovery and must remain
visible in the evaluation limitations.

`backgroundProtected()` exposes held or credible pending musical evidence to an
upstream noise observer. A gate opening alone is not musical evidence. This
getter adds no observation window; the full preconditioning/dynamics signal
path needs its separate integration and acoustic measurements.

## Rejected changes retained

* [Held continuity](held-continuity/REPORT.md): one variant reduces extra Ons
  but extends unwanted noise duration and creates genuine-note release errors.
  No held-continuity fallback was adopted.
* [Odd-mode censoring](odd-mode-censor/REPORT.md): fitting only observed odd
  partials reduces some false octave cascades, but loses B3 and ends E3 early
  in a known six-source E-major control. No odd-envelope change was adopted.

These decisions retain misses, extra notes, premature/late releases, false
voice durations and raw causal timestamps. Fewer On events alone can conceal
longer unwanted ringing. All corpora used here are seen development data.

## Integrity and reuse

Each subdirectory preserves its original `manifest.json` and every text file
listed by `files_sha256`. Executables, object files and duplicated PCM are
excluded. `retention.json` records each original manifest's hash and copied
byte count. Verify a subdirectory by hashing the exact relative paths in its
manifest. The top archive inventory additionally covers this README.

Original absolute `/tmp` paths in scripts and capture metadata describe the
historical run. They have not been rewritten. In particular, the accepted
support experiment reused the byte-identical baseline from the odd-mode audit;
the original `/tmp/flysnapper-odd-censor-audit/baseline` maps to
`odd-mode-censor/baseline` here. `reused-baseline.json` binds all 37 reused
baseline files, while the accepted archive also preserves its referenced raw
copies. Reproduction uses fresh output directories and the retained source
snapshots, not a later working tree.

The measurements are standalone 48 kHz/64-sample cloud detector captures.
They are not physical interface latency, a Windows/Ableton run, or validation
of the subsequently integrated raw-noise observer and dynamics frontend.
