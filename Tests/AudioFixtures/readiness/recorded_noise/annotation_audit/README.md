# Signal-content and annotation audit

This independent audit does not run the detector, change labels, or alter the
original 37/128 candidate result. The original recordings, source mappings and
sealed controls remain unchanged. It examines two different failure types.

## A false repeated attack over a ringing note

The retained MIDI57 + acoustic `mute_strings1` mixture at nominal 12 dB SNR,
common gain 0.04, produces a false Off57/On57 at sample 50175 (1.0453125 s).
The target is intended to remain the same note from 0.25 to 1.35 s.

The independent script reconstructs the separate desired and unwanted source
components using the recorded derivation. Their sum agrees with the retained
PCM16 to half a quantization step (maximum absolute difference 1.52589e-5).
Thus the component comparison does not depend on a source-separation model.
Those components are known only because this is a constructed benchmark; a
real detector receives their sum.

For the 32 ms Hann window ending at the emitted event, the following are
deterministic component projections at multiples of nominal 220 Hz:

| Partial | Desired/unwanted projection ratio | Mixture phase change from desired alone |
| --- | ---: | ---: |
| 1 | 37.1 dB | -0.72 degrees |
| 2 | 32.9 dB | -0.23 degrees |
| 3 | -14.3 dB | -20.1 degrees |
| 4 | 7.9 dB | 11.3 degrees |
| 5 | 12.3 dB | -13.5 degrees |
| 6 | 0.4 dB | 10.1 degrees |
| 7 | -9.5 dB | -82.4 degrees |
| 8 | -7.4 dB | -1.20 degrees |

The strong first two partials preserve evidence of the ringing target.
Counting changed weak upper-partial phases as separate confident attack votes
would be unsafe. Conversely, partial 8 happens to have a small phase change
even though the unwanted component dominates it: phase agreement alone is not
proof that a weak target component remains observable.

The nominal SNR is RMS over the entire note-active interval, not a constant
local SNR. A trailing 32 ms time-domain window reaches approximately 0.01 dB
near 1.0 s; at the false event the corresponding local SNR is 10.9 dB.
The report retains 16/24/32/48 ms past-only windows and the local trajectory.
Projected component ratios are not statistical confidence intervals for this
correlated percussion interference.

These are historical weighted windows. The 32 ms window's energy center is
1.02932 s, about 16 ms before the decision. It cannot establish instantaneous
phase at the final sample, nor prove what a proposed causal observer would
know after filtering/conditioning. No future samples enter those prefix
windows; the separately described offline tail analysis below does use future
data and is marked accordingly.

## Author-labelled electric percussion is not spectrally empty

The two mono noise-only failures are the two common-gain versions of
`Samples-electric-perc_4_rr1_2.wav`. Both emit MIDI47 at sample 44159
(0.919979 s), 119.98 ms after the second sample playback starts.

The original unfiltered PCM24 source contains ringing resonances. In a Hann
window from source time 0.02 s to that available 0.11998 s prefix, prominent
peaks are near 369.01 and 494.30 Hz, alongside a strong 296.44 Hz mode. The
first pair lies near third/fourth partials of approximately 123 Hz, close to
MIDI47, but is not proof of one harmonic source or of a musician intending
that note. Offline characterization of source times 0.05–0.35 s also finds
ringing near 369.15 and 494.12 Hz. That longer window contains future samples
relative to the false On and supplies no causal detection claim.

The author maps this recording through `electric_noises.sfz`; its sample key
27 selects a percussion sound and is not MIDI pitch truth. The sealed edition
assigns suppression intent to these gestures. Both failures remain failures
under that policy. The label does not imply the sound is wholly non-tonal,
and cannot support a universal claim that every pitched muted/percussive
gesture can be distinguished from an intended quiet note using audio alone.

## Reproduce

```sh
python Tests/AudioFixtures/readiness/recorded_noise/audit_signal_content.py \
  --output /tmp/flysnapper-recorded-noise-signal-audit
```

The script needs NumPy and FFmpeg for the same sample-rate conversion used by
the recorded derivation; the original electric percussion FFT uses the raw
44.1 kHz PCM24 bytes directly. `audit-manifest.json` binds all inputs, the
script and result to their hashes and records tool versions. It never imports
or executes Flysnapper DSP. The fixed data seals and scoring policy are not
rewritten by this audit.
