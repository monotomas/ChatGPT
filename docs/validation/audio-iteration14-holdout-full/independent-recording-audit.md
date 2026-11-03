# Independent audit of five electric-guitar holdout recordings

Date: 2026-11-03. This is a read-only analysis of the saved iteration 14 holdout
measurements and original audio. The detector was not executed, tuned or changed.
No fixture, annotation or existing measurement file was changed.

## Finding

The evidence supports **unwanted same-pitch retriggering during sustained,
changing timbre** as the explanation for these five cases. There is no obvious
new acoustic attack corresponding to the repeated output notes. Their 198 extra
Note Ons account for **84.6% of the 234 reported mono false Note Ons** across the
nine rate/block configurations. This does not explain the remaining 36 extra
Note Ons, which belong to `hidden_modal_07_mono` (27) and
`hidden_modal_14_mono` (9).

All 198 extras in the five recordings are preceded immediately by a Note Off
of the same pitch **at the identical sample**. The audio continues through these
zero-gap MIDI restarts. The repeated notes retain the correct recorded pitch;
they are not new pitches or octave changes in these cases. The first correct
interval is consequently scored as ending early. This is evidence of note
fragmentation, not evidence of a new physical string release at each Note Off.

## Stored events and input integrity

The event times below are fixture time at 48 kHz / 64 samples. Subtract 50 ms to
obtain time within the original source crop. They list extras after the first
correct Note On, which occurs at 101.31–103.98 ms.

| Fixture | Original source | MIDI note | Extra Note Ons per configuration | Extras across nine configurations | Extra Note On times, ms |
| --- | --- | ---: | ---: | ---: | --- |
| `hidden_recorded_01_mono` | `electric_E2.ogg` | 40 | 5 | 45 | 199.98, 295.98, 367.98, 453.31, 538.65 |
| `hidden_recorded_02_mono` | `electric_Fs2.ogg` | 42 | 6 | 54 | 194.65, 269.31, 343.98, 421.31, 495.98, 647.98 |
| `hidden_recorded_03_mono` | `electric_A2.ogg` | 45 | 6 | 54 | 197.31, 279.98, 362.65, 461.31, 533.31, 615.98 |
| `hidden_recorded_04_mono` | `electric_C3.ogg` | 48 | 3 | 27 | 194.65, 269.31, 346.65 |
| `hidden_recorded_05_mono` | `electric_Ds3.ogg` | 51 | 2 | 18 | 194.65, 271.98 |

The counts are identical at 44.1/48/96 kHz and block sizes 32/64/128. Comparing
corresponding emitted onsets across all nine configurations gives a maximum
physical-time spread of 0.0163 ms. These are repeatable musical errors across
configurations, not a single unstable timing run.

All five original Ogg and all five derived WAV SHA256 values match their
checked-in manifests. Independently decoding each original Ogg with FFmpeg to
mono float32 at 48 kHz, multiplying by 0.35, retaining the first 31,200 samples,
applying the declared 192-sample terminal taper, rounding to PCM16, and adding
2,400 leading and 12,000 trailing zero samples reproduces every derived WAV
sample exactly: **zero PCM-sample difference for all five files**. No edit at any
interior repeated-note time was introduced by this derivation.

Recomputing the declared absolute-amplitude onset rule, `abs(sample) >= .001`,
on the scaled float audio gives fixture samples 2402, 2401, 2400, 2406 and 2401,
respectively, exactly matching the five annotations. All annotated endings are
sample 33,600, or 700 ms in fixture time. This is the artificial crop ending;
the last 4 ms of source audio are tapered. The unmodified source files are
approximately 4.219 seconds long.

## Independent acoustic checks

I inspected the WAV waveforms, 20 ms RMS amplitude envelopes, 10 ms RMS envelopes
after a fourth-order 3 kHz high-pass filter, and 85.33 ms Hann-window
spectrograms with 5 ms hops. I also checked the decoded original source crops,
which contain the same continuous tone. This is numerical and visual analysis;
it is not a listening-panel judgment or a human performance annotation.

There is one strong initial broadband attack. The sustained waveform then
decays, with small periodic envelope fluctuations and changing partial balance.
The repeated output onsets do not coincide with a silence/restart, a broad
envelope reset, or a renewal of the initial broadband attack. The continuously
resolved harmonic tracks do not reset at the repeated-note times.

The following envelope measurements summarize the acoustic evidence. The RMS
rise column uses a centered 40 ms moving RMS and measures the largest increase
over 20 ms at any center from 100 to 680 ms, covering the repeated notes and
their preceding attack-search regions. The HF column takes the largest >3 kHz
10 ms RMS in any of the 60 ms intervals preceding an extra Note On and compares
it with that recording's initial HF peak in fixture time 50–130 ms. A 60 ms
lookback includes the approximately 51–54 ms initial detector response delay.
These are descriptive diagnostics, not newly chosen acceptance thresholds.

| Fixture | Largest 40 ms RMS rise over 20 ms | Largest precursor HF level relative to initial HF peak | Lowest 20 ms RMS in fixture time 150–680 ms, relative to initial RMS peak |
| --- | ---: | ---: | ---: |
| `hidden_recorded_01` | +0.868 dB | −24.5 dB | −10.9 dB |
| `hidden_recorded_02` | +0.666 dB | −17.6 dB | −13.6 dB |
| `hidden_recorded_03` | +0.326 dB | −19.3 dB | −14.5 dB |
| `hidden_recorded_04` | +0.192 dB | −19.9 dB | −14.9 dB |
| `hidden_recorded_05` | +0.045 dB | −21.6 dB | −11.8 dB |

There are some low-level high-frequency swells, notably around fixture time
350–400 ms in Fs2 and A2. They remain far below the initial attack, do not reset
the overall amplitude envelope, and occur on top of the continuing harmonic
tone. Their presence prevents a claim that the timbre is perfectly stationary;
it does not supply evidence for the five or six repeated physical plucks that
the output would imply.

The pitch family remains stable. As a diagnostic independent of the product
detector, I calculated autocorrelation period estimates on 100 ms windows of
the **original decoded source**, centered every 25 ms from 100 through 600 ms.
The local lag search covered frequencies within ±6% of the labeled fundamental,
with parabolic interpolation. These estimates are conditional on that octave
choice; the separate unconstrained FFT peaks below support that choice.

| Source | Labeled fundamental | Estimated fundamental range | Cents relative to label | Minimum normalized correlation at the selected period |
| --- | ---: | ---: | ---: | ---: |
| E2 | 82.407 Hz | 82.298–82.423 Hz | −2.3 to +0.3 | 0.9927 |
| Fs2 | 92.499 Hz | 92.370–92.634 Hz | −2.4 to +2.5 | 0.9971 |
| A2 | 110.000 Hz | 109.908–110.296 Hz | −1.5 to +4.6 | 0.9972 |
| C3 | 130.813 Hz | 130.640–131.178 Hz | −2.3 to +4.8 | 0.9981 |
| Ds3 | 155.563 Hz | 155.227–155.739 Hz | −3.7 to +2.0 | 0.9920 |

Hann-window spectra of source time 60–240 ms and 400–620 ms show the same
fundamental/harmonic families at both times. For example, the strong E2 peaks
are approximately 82, 164, 247, 329 and 412 Hz; Fs2 peaks are approximately
92, 185, 278, 370 and 462 Hz. The strongest early A2 peak is its second harmonic
near 220 Hz, while its late fundamental near 110 Hz becomes strongest. C3
similarly changes from a strongest second harmonic near 261 Hz to a strongest
fundamental near 130 Hz. This supplies direct evidence of changing timbre on a
sustained pitch, without changing the reference note identity. Higher partials
generally decay much faster than the sustained lower partials.

## Interpretation and limits

For these five edited isolated-note samples, the single-note annotation is
supported by the retained pitch family, continuous decaying envelope, lack of
obvious renewed attacks, and exact source-to-fixture reconstruction. The
observed failure is repeated same-pitch MIDI restart during that continuing
tone. Evolving harmonic balance and small envelope fluctuations are plausible
inputs to the unwanted retrigger decision. This audit does not identify the
responsible internal code branch, because no internal decision trace was
recorded or detector rerun performed.

The source library identifies isolated notes, not human-labeled pick gestures
or physical finger-release times. It also describes upstream trimming,
normalization, denoising, ramps and sometimes pitch correction. Stable pitch
alone cannot exclude a very soft same-pitch repick, and the present envelope
checks cannot prove the absence of every physical gesture in the original
recording session. There is no human reference annotation with which to settle
such a claim. The defensible result is that there are **no obvious additional
attacks that explain the stored repeated outputs**, with positive acoustic
evidence for a sustained decaying note instead.

The annotated Note Off at 700 ms is an imposed acoustic cutoff after the
650 ms source crop. It is suitable for measuring the detector's response to
that edited cutoff, not for inferring a player's intended musical Note Off.
The recorded onset is also an amplitude-rule annotation rather than a measured
physical pick-contact time. These limitations must remain attached to release
and latency interpretations.

Nothing in this audit turns the holdout into a passing acceptance test,
changes any error count or annotation, or certifies the user's physical
5/10 ms latency goals. The remaining synthetic mono errors, polyphonic errors,
misses and callback deadlines require their own evidence.

## Reproducibility and input identity

Analysis used Python with NumPy 2.3.5, SciPy 1.17.0 and FFmpeg 7.1.5. Moving RMS
is `sqrt(max(0, convolve(x*x, ones(N)/N, mode='same')))`, with N=960 for 20 ms,
N=1920 for 40 ms and N=480 for the HF 10 ms envelope, at 48 kHz. The high-pass
filter is `scipy.signal.butter(4, 3000, fs=48000, btype='highpass', output='sos')`
followed by `sosfilt`. Spectra used zero-padded 131,072-point FFTs after Hann
windowing; this padding interpolates the display and does not imply that
frequency resolution equals the padded-bin spacing. The pitch table is a
diagnostic estimate with the stated window and lag restrictions, not an
independent cents-accuracy certification.

Input SHA256 values at audit time:

```text
Tests/AudioFixtures/holdout/fixtures.json
816faff8b414f579e2b107057113c0c34df8685abfc768d26c978197e6e7d692
Tests/AudioFixtures/holdout/tonejs/manifest.json
feee660df57a5eadc38d0fa5236b4690e150770878b63088bfca8a93910ec04e
audio-cases.csv
5f7994ebc1448a5754f92583173935c808ee2ed882f95efa059bedd7a2997870
audio-events.csv
c0af1f2b1b207aaf7b4dffa19597c2bd134fb47c99d48c929853d5c89c908f5c
measurement-manifest.json
a0c5e55b11de05cae6da92077bf3eb5a2a89f10c348a9d5b4ee4908eff5b0a42
```

Sources: [fixture metadata](../../../Tests/AudioFixtures/holdout/fixtures.json),
[original source manifest](../../../Tests/AudioFixtures/holdout/tonejs/manifest.json),
[declared preparation and limitations](../../../Tests/AudioFixtures/holdout/README.md),
[preparation code](../../../Tools/audio_validation_holdout.py),
[stored case measurements](audio-cases.csv), and [stored events](audio-events.csv).
