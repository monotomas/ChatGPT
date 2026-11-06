# Rejected spectral-envelope alternative: fewer Ons can conceal more omissions

This is a **rejected standalone detector experiment**, not a product change or
release recommendation. Production `Source/`, test policies and existing sealed
archives were not changed. No new real recording or reserved audio was used.
There are54 diagnostic configurations per implementation,108 baseline/candidate
captures in total. Reproduction reuses the same disclosed configurations.

## Candidate and supplied information

The candidate replaces free complex harmonic coefficients with a fixed analytic
power-spectrum dictionary. Each possible MIDI source has twelve envelopes:
spectral slopes `.8/1.2/1.8/2.6`, each with even/odd amplitude ratios
`.15/1/6.6667`. Eight partials are convolved with the exact 512-sample Hann
power response. At most six sources are selected, with one profile per note;
eight bounded NNLS sweeps refit only one nonnegative scalar per source. The
fixed improvement floor is `.006` of observed squared-power energy. No chord,
wanted voice count, reference note or probe labels are passed to the detector.

The caller supplies its actual post-conditioning/dynamics FFT power. Its
512-sample/12-kHz observation and existing note confirmation remain unchanged.
The dictionary discards relative phase. It **cannot provide the analytic
waveform prediction** used by the original polyphonic innovation observer and
leaves `predictionValid=false`. This is an explicit control-path confound:
fewer repeats must not be attributed solely to a better harmonic prior.

## Dense open-string rejection

Thirty configurations cover the clean six-open-string strum with five common
detunings, three timbres and dynamics off/default. The six starts are
`80/87/94/101/108/115 ms`; each source has one attack and a smooth decay.

| Onset-count diagnostic | Exact 0.3.0 baseline | Spectral candidate |
| --- | ---: | ---: |
| Wanted notes found / expected | 30 / 180 | 24 / 180 |
| Missing wanted notes | 150 | 156 |
| Extra Note Ons | 32 | 18 |
| Repeated nominal Note Ons | 8 | 0 |
| Onset-count-error-free configurations | 0 / 30 | 0 / 30 |

At zero detuning/normal timbre/default dynamics, 0.3.0 finds only40/50/55,
repeats40 and invents43; its first40 is87.979ms late. The candidate instead
sends **only unplayed MIDI52**, at154.646ms absolute time. None of the six
wanted notes is emitted. Reduced false-trigger totals are not improvement
when wanted notes disappear. Short-window blended fundamentals, interference
and the phase-averaged envelope model remain unresolved here.

The fresh baseline open-string stream is byte-identical to case10 of the
other audit's retained330-case capture. Only these30 configurations were run
for this candidate; the full330/132 matrices were not relabelled as executed.

## Additional physical falsifiers and the timing trap

Twenty-four further configurations cover a normal single source, even-dominant
single, smooth odd/even morph, twelve-partial single, late-added octave and
late-added near-third source. Each uses dynamics off/default and two fixed
source-phase variants. There are32 wanted notes. Both implementations emit
all32 nominal targets; the candidate has no extra/repeated nominal Onsets,
versus8 octave extras and2 repeats in the baseline.

That nominal count **does not establish successful polyphonic acquisition**:
only2/8 baseline late additions and **0/8 candidate additions** are usable
while both physical sources actually overlap. For example, candidate52 starts
at746.646ms although its source starts at280ms; it is admitted only after the
old source releases. Eight candidate first releases are more than80ms after
the synthetic physical cut, versus none in the baseline. These failures and
the disabled prediction path remain in `timing-release-audit.json`.

These are disclosed diagnostics, not strict full-policy case passes or real
guitar/physical-interface latency measurements. Actual decision samples are
retained without alignment or backdating. No threshold was tuned to repair
the central chord after seeing its failure.

## Runtime, exact bytes and reproduction

The24 serial falsifier runs each supply48-kHz audio in64-sample callbacks.
Candidate worst per-case callback P99 was1382.012us against a1333.333us budget;
57 observed callbacks missed that budget. Baseline worst per-case P99 was
689.912us, with one observed miss. This shared-cloud timing is not a certified
realtime bound or proof of the intrinsic cost of every envelope method.

`source-inputs.json` binds16 selected Pitch files to Git source
`e235c8a098dadce6e0a4b8bd6ca9de789a5d174d`. Seven C++ translation units were
actually compiled. The three changed candidate files are retained under
`candidate-source/`; other bytes are recovered from that exact Git commit.
This is a selected project inventory, not full compiler/header dependency
closure. Execution records retain compiler/commands and executable hashes;
executables, PCM, generated dictionary tables and model weights are excluded.

With that Git commit present locally and `g++` available:

```sh
python docs/validation/guitar-reliability-2026-11-06/spectral-template-negative/reproduce.py \
  --project-root /workspace/ChatGPT --output /tmp/new-spectral-template-diagnostic
```

The output directory must be new. The runner restores exact source bytes,
compiles/runs both implementations, and writes the nominal-count and actual
overlap/release audits. Reproduction was executed; all four raw event streams
matched the archived measurements byte for byte. CPU timings naturally vary.
