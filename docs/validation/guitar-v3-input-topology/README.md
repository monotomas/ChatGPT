# Input dynamics topology and bounded guitar-evaluation proof

This is retained development evidence for the revised November 5 scope, not a
claim of general musical readiness. The final frozen prototype is
`analysis-after-nc-precise-reference`. Production subsequently adds separate
calibration lifecycle flags/helpers; those additions need their own native
processor tests and are not covered merely by these standalone captures.

## Signal path

The original DC blocker, lowpass, causal resampling and noise conditioner
continue to receive raw input. The gate/expander, compressor and limiter then
shape the **cleaned analysis-rate samples**, before the actual pitch ring,
FFT and YIN candidate. This lets the original observer learn background while
the gate is closed and prevents subtracting a raw hum model in a different
compressed/gated amplitude domain. Original host-rate attack envelopes and
velocity strength remain raw; dynamics does not flatten MIDI velocity here.

A parallel cleaned reference ring, before dynamics gain, keeps learned-floor
and harmonic/phase evidence in the same measured domain as the observer. During
fresh acquisition/rearticulation, a nearby supported reference F0 can refine
the processed hypothesis within half a semitone; it cannot invent another
octave. The existing same-rounded-note quarter-tone precision check is retained
for short periods before any reference replacement. Without that last check,
an earlier prototype lost six high-edge detuned notes; the failure is preserved.

During partial polyphonic history, existing fresh-mono harmonic/phase/SNR
admission evidence plus a physical root and three partials can protect a pending
string from being absorbed during the learner's preceding-attack cooldown.
This protection does not emit MIDI: full polyphonic observation and confirmation
remain required. No admission, noise or release thresholds were lowered.

## Complete comparative results

Both editions are seen development data at 48 kHz/64 samples. The controlled
capture contains 72 historical configurations; seven poly-bend configurations
are explicitly outside the user's revised scope and are not counted as passes.
Old fixture bytes and policies are intact. The dated scope adapter reports
latency descriptively and retains misses, extras and release errors.

| Metric | Host dynamics before observer | Final dynamics after observer |
| --- | ---: | ---: |
| Controlled configurations passed | 35/65 | 42/65 |
| Controlled wanted notes matched | 130/151 | 133/151 |
| Controlled extra Ons | 84 | 74 |
| Controlled early / late releases | 9 / 4 | 8 / 6 |
| Recorded-noise configurations passed | 47/128 | 48/128 |
| Recorded-noise wanted notes matched | 96/96 | 96/96 |
| Recorded-noise extra Ons | 245 | 240 |
| Recorded-noise early / late releases | 45 / 3 | 43 / 3 |

All eight monophonic quiet-hum controls are clean in the final topology. The
independent [audit](independent-audit-20261105/REPORT.md) retains the full
per-reference changes. Some recovered polyphonic notes still release late:
the SNR12/gain1 example releases about 638 ms late; the SNR24 cases about
89 ms late; quiet rearticulation about 96 ms late. These are material failures,
not successful musical acceptance hidden by aggregate recall.

Noise-only false **voice duration** improves from 7.520 to 5.912 seconds in the
controlled set, while individual noise Ons increase from 24 to 32. Recorded-noise
voice duration improves from 6.827 to 6.699 seconds, but its union duration
slightly worsens from 4.984 to 4.997 seconds. Voice duration sums concurrent
voices; union duration measures time with at least one unwanted voice. Both
views remain visible. Factory thresholds were used here; a guided calibration
or manual gate adjustment is not simulated by silently choosing settings from
these reference labels.

## Repicks and detuning

The original three overlapping, genuinely re-excited +30-cent A3 notes were
tested at 44.1/48/96 kHz and both 2/12-semitone ranges. The final prototype
produces three correct anchor-57 Ons and three balanced Offs in every stream.
Every On is preceded by a same-sample pitch event. First decision at 48 kHz is
18.65 ms; repeated pitches are about 57.302 and 57.295. This verifies causal
detector output, not physical hardware latency or the later processor adapter.

The [precise-reference addendum](independent-audit-20261105/PRECISE_REFERENCE_ADDENDUM.md)
checks all 876 combinations of MIDI28–100, ±40 cents, pure/bright tones and three
sample rates. Final and dynamics-disabled baseline each pass 861/876, with the
same 15 pre-existing cold near-mains misses and no new misses, wrong Ons or
voices remaining after silence. Six note100/+40-cent failures in the earlier
coarse-reference prototype are fixed, with their original baseline On samples
restored exactly. The old negative report and 21-miss data remain unchanged.

Among 861 jointly matched detuning cases, 817 onset times are identical, 21 are
slower and 23 faster. The range of change is approximately −2.68 to +53.33 ms;
low bass accounts for the largest delays. Misses remain separate from those
conditional timing statistics. No event or pitch curve is backdated or shifted
to improve its comparison with truth.

## Retained iterations and reproduction

Every named directory retains its exact source, capture driver, raw events,
scored cases and logs. Earlier coarse/reference/strict-protection failures are
retained alongside the accepted prototype. `debug-*` directories explain the
contaminated-floor acquisition failure; their instrumented timing is not a CPU
benchmark. The independent audit reconstructs all matched/release decisions
with zero discrepancies and preserves its original report before the final
precision addendum.

`retention.json` identifies excluded compiled binaries by hash. No executables,
object files, model assets or duplicated PCM are included. Original absolute
temporary paths remain historical provenance; compile the retained source with
the corresponding driver into a fresh scratch directory. The `reproduction/`
tree retains scorer, policy and input inventories for replaying the saved raw
events. It is explicitly a post-run retention copy, not a claimed contemporaneous
scorer-capture manifest. PCM remains in the original sealed repository fixtures.

`archive-manifest.json` binds every retained file. These measurements do not
establish Windows, Ableton, audio-driver latency, universal source/noise intent
classification or arbitrary guitar technique coverage. Final native, processor,
compiled-plugin and installer evidence belongs to the separately frozen product
build, not to these prototypes.
