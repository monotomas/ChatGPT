# Guitar behaviour controls

These controls address the user's observed early noteslips, sensitivity to
input strength and changing guitar timbre, false octaves, and missing continuous
bend behaviour. The generator does not read detector output. The42 PCM files /
55 mode cases were sealed before the first source19 measurement, then disclosed
to the DSP implementation owners. They are now known regression controls,
not new unseen-data acceptance evidence.

The pre-evaluation seal is retained in `SEALED_SHA256.txt`. `verify.py` checks
PCM hashes, annotation/inventory agreement, source recordings and the saved
original baseline using Python's standard library. `generate.py --output PATH`
reproduces the files separately using NumPy/FFmpeg; ordinary builds use the
retained PCM and need neither generator dependency.

The physically controlled cases include48dB decays to low but still periodic
tails, evolving odd/even partial balance, stiff-string inharmonicity, three
identical waveforms at different gains, actual separately articulated octave
changes, damping followed by broadband noise, and noise with no pitched source.
The C++ test checks complete source intervals, extra/missing notes,
release timing and invariance to host block segmentation. Its90ms acquisition
ceiling is a basic functional bound and does not establish the5/10ms physical
latency goal. Continuous bend controls carry fractional pitch truth every64
samples in `pitch-curves.tsv`; scoring those as fixed notes is diagnostic only.
With mono bend enabled, all four controls run at2/12-semitone bend ranges and
64/257-sample blocks. The test requires one anchored note, equivalent pitch
decisions across block sizes, complete curve coverage, per-curve P95 absolute
error at most35 cents and a75-cent worst transient bound. Settled segments
must stay within20 cents; moving-segment maxima are reported separately so
rapid bends are not hidden in an aggregate score. These bounds were declared
before evaluating the changed detector. The separate mandatory processor test
checks delivery as actual MIDI pitchwheel/RPN/channel events.

Eight extended recorded cases reuse four **previously disclosed** Tone.js guitar
sources at two gains. Their1.55s natural tails extend beyond earlier650/700ms
crops, but these are not claimed to be new recordings. Source filenames/hashes
are in `fixtures.json`. The original samples remain in the neighboring fixture
source folders with CC-BY-3.0 notices. Acoustic samples originate with the
University of Iowa, electric with Karoryfer, and nylon with Freesound contributor
quartertone. Upstream: `nbrosowsky/tonejs-instruments`, commit
`622c2f1c32c8cfce4158ddc3eb26e518ddef37e5`. Synthetic controls and generator:
AGPL-3.0-or-later.

`baseline-source19` preserves the original source19 evaluator output and exact
source/evaluator hashes. At48kHz/64 samples, all14 dynamic-decay cases ended
early. Seven mono and seven poly evolving-timbre/stiff-string cases acquired
the correct initial note but then emitted a false note **one octave higher**.
This reproduces harmonic sensitivity; it does not reproduce the specifically
downward octave direction reported by the user. The recorded extended tails,
gain controls, true octave switches and negative/noise cases passed this first
baseline. The four continuous bend cases produced seven extra NoteOns under
fixed-note operation, which lacks true pitchwheel output.
