# Recorded muted/percussive and quiet guitar controls

This edition was selected, labelled and sealed before running any Flysnapper
detector on it. Its frozen c5b baseline is retained under `baseline-c5b/expression`.
The first unseen candidate capture is now complete against frozen monophonic
checkpoint v19 plus MAIN poly model 17, with unchanged sources and verified seal.
Only after completion were failures shared with implementation owners. These
inputs are now **seen regression data**, not future unseen evidence.

Seal: `0d850ff509fffc2aca710a51301db2984f0350b0e573847dfb3ca20e6498766d`.
The 64 retained WAVs produce 128 mono/poly mode cases under the unchanged parent
policy. Input PCM, original recordings, SFZ maps, legal text and source hashes
are retained. `derive.py` never imports or runs the detector.

Source: [Karoryfer Shinyguitar](https://github.com/sfzinstruments/karoryfer.shinyguitar),
commit `57243cca85277dbcc120ce17c6178032f93c80f3`, CC0-1.0. D. Smolken played
and mapped the archtop guitar. The source author documents microphone and
magnetic pickup recordings. They are paired views of the same named gestures;
they must not be counted as independent players or performances.

The author places muted-string and percussion recordings in
`Programs/acoustic_noises.sfz` and `Programs/electric_noises.sfz`. Their assigned
low MIDI keys are sample-selection controls, not pitched note truth. These
particular author-labelled percussive/muted gestures are assigned suppression
intent for this edition. This does not imply that every quiet or partly damped
pitched string is unwanted, and does not establish real squeak or mains-hum
coverage.

The quiet positive notes are original lowest-velocity recordings, independently
mapped by the author's SFZ to MIDI 40 and 57. Their file names use a different
octave convention from the older black/green guitar library. Fractional pitch
truth is unavailable, so nominal SFZ cents are diagnostic rather than a
zero-cent oracle. Source sample playback time is known; the original physical
gesture's time before the audio edit is unknown.

Controls include two common gain levels for each noise-only source, quiet
desired notes beginning immediately at sample zero and after 350 ms of silence,
and desired-note/recorded-noise mixtures at 6/12 dB SNR with paired common gains.
SNR is defined by separate source RMS over the complete known note-active
interval, before the common gain. The sample-zero controls prohibit interpreting
a mandatory noise-learning wait as satisfactory onset behaviour.

The mixtures contain actual recorded noise and actual recorded notes assembled
at controlled times. They are not claimed to be natural simultaneous human
performances. These sources do not provide real continuous fretted slides,
hammer/pull transitions, bends or independently bending polyphonic voices.

## First candidate results

The captured source is `/tmp/flysnapper-checkpoint-v19`, retained exactly under
`snapshots/mono-v19`; raw output and hashes are in `candidate-mono-v19`.
This **monophonic checkpoint v19 with MAIN poly17** is different from the isolated
phase-prior19 prototype in the separate oracle tracking audit.

The full policy has 37/128 passing runs, 34/64 mono and 3/64 poly. This is a
failing checkpoint. The matched-note mono latency is median 29.98 ms / P95
45.98 ms, and poly 55.31 / 69.98 ms. These are measured causal detector response
times relative to sample playback, not original physical-gesture timing.
The edition contains no continuous fretted/hammer transitions, so that latency
checkpoint is explicitly unrun, not passed.

| Mode / family | Candidate pass | Additional note-ons | Early first offs | Late offs |
|---|---:|---:|---:|---:|
| Mono quiet real plucks | 16/16 | 0 | 0 | 0 |
| Mono recorded noise only | 14/16 | 2 | — | — |
| Mono note/noise mixtures | 4/32 | 48 | 28 | 0 |
| Poly quiet real plucks | 0/16 | 8 | 4 | 6 |
| Poly recorded noise only | 3/16 | 65 | — | — |
| Poly note/noise mixtures | 0/32 | 96 | 16 | 12 |

Every intended note was found. The ongoing quality failures are precision,
unwanted rearticulation and retention. Mono's 48 extra mixture onsets include
44 repeated correct anchors and four notes 19 semitones below the target.
For example, desired MIDI57 from 0.25–1.35 seconds produces On57 at 0.26665,
Off57+On57 at the same 1.04531 seconds, then Off57 at 1.38931, after a recorded
muted-string gesture starting at 0.95 seconds. An early first Off can therefore
mean a spurious retrigger, not necessarily a silent gate/dropout. Poly mixtures
also contain 20 octave errors; their extras span many incorrect pitches.

Direct comparison with c5b shows a mixed result. Mono mixture extras improve
68→48, octave errors 2→0 and early offs 32→28; clean real plucks remain 16/16.
Mono noise-only false onsets improve 4→2, while affected-case coverage worsens
from 15/16 passing to 14/16. Poly noise-only false onsets regress 20→65. Poly
mixture extras improve 130→96 and octave errors 28→20, but early offs worsen
14→16 and late offs 0→12. No hanging voices or output overflow were observed.
The baseline's missing poly-bend capability also affects its pass count, so
31→37 aggregate passes must not obscure these direct regressions.

`compare.py` reproduces `comparison.json` with complete family/mode metrics and
raw hashes. Numerical policy, intended labels, PCM and original failure reports
remain unchanged. No musical-readiness approval follows from this capture.
