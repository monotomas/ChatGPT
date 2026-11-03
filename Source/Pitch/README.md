# Audio-to-MIDI implementation

The detector is a separate technical part of Pitch-to-MIDI. Its generated notes
enter the same correction module as external performance MIDI; it is not a third
user-facing processing module. `PitchDetector.h` is the integration contract.

## Causal processing

`prepare` builds frequency/FFT and harmonic regression tables outside audio
processing. The internal analysis rate is 12 kHz (or the host rate below
12 kHz). A bounded phase accumulator interpolates between two filtered host
samples already received. This makes the observation interval equal at
44.1/48/96 kHz and delays a fractional decision by less than one host sample;
it never reads future audio. A rate-dependent 12 Hz DC blocker and four
cascaded one-pole low-pass stages precede resampling. The
filter coefficient scales with host rate; a fixed coefficient must not be used
because it would increasingly remove bass notes at high host rates.

Every 32 internal samples a decision is made from audio already received.
Events carry the host-sample position of that decision, not the earlier acoustic
attack. No future block or lookahead is requested. Frames of historical audio
are an actual observation interval, not evidence of zero pitch latency.

## Monophonic estimator

The production estimator uses causal YIN cumulative-mean-normalised difference
with a comparison length that is constant over the tested lags. The primary
frame has at most 512 internal samples. Very low notes can use a longer past-only
fallback frame, up to 1024 samples, with strict confidence. The declared range
is MIDI 28 through 100, A4=440 Hz; independent full-range tests cover this range.

A guitar attack can contain a body resonance plus a substantially stronger
harmonic series. Their low common divisor can fool an otherwise high-confidence
period estimator. A weighted harmonic-series check prefers a higher series only
when its partials dominate the measured power; see `YinEstimator.md`. This is a
generic timbre assumption with measured tests, not a proof for arbitrary audio.

New decisions require three consistent estimates and a short continuous-pitch
stability interval. Stability is measured across estimates, not by a permanent
fractional-semitone dead zone: a stable note detuned by 40 cents remains valid.
Held notes use 62-cent hysteresis against vibrato-triggered note chatter. There
is no generated pitch bend; meaningful pitch changes create new MIDI notes.

A same-pitch repick can have little amplitude change. Monophonic repicks are
therefore also detected from bounded one-period prediction innovation. The
prediction period follows continuous measured pitch, independently of the
quantised MIDI note. Ordinary amplitude onsets use period-dependent smoothing:
a bright bass waveform maximum must not become a new musical attack. Following
an innovation the old voice is retained while fresh post-attack audio is
estimated. This avoids retriggering a stale pitch when the actual new attack has
a different pitch. A falling-input veto suppresses release transients. In polyphonic mode, this
repick path is restricted to exactly one held voice with a matching stable
periodic anchor. It waits for a complete fresh observation and confirming hops
before ending and restarting that note; an added voice or changed pitch cancels
the pending repick.

## Polyphonic estimator

`TimeDomainPursuit` fits free-phase harmonic groups and refits their shared
partials jointly. It can use upper partials even when neighbouring fundamentals
cannot be separated by the short FFT. The FFT is used for independent tonality
and acoustic-innovation checks. A stable, confident YIN series can anchor the
harmonic pursuit when a merged fundamental would favour a neighbouring note.
A newly proposed anchor requires an observed fundamental or both coprime
upper partials 5 and 7; an arbitrary common divisor of played notes is insufficient.
Competing groups require physical partials independent of that series, with
the separate octave-envelope prior retained for shared octave voices. These
are guitar-oriented timbre assumptions, not universal source-separation proofs.
Algorithm and numerical details are
in `TimeDomainPursuit.md`.

Polyphonic processing observes a trailing 512-internal-sample frame. At 48/96 kHz
host rates that is 42.67 ms of audio. First decisions also require six
consistent estimates with a supported periodic anchor, otherwise ten.
This is explicitly incompatible with a general 5/10 ms live-latency claim.
Independent measurements, including failures, are in `docs/AUDIO_VALIDATION.md`.

Shared harmonic models can otherwise invent new voices while an old sound
changes timbre during decay. New polyphonic voices are admitted during an
initial acoustic acquisition interval. Renewal uses a bounded prediction of
the next 32 internal samples from the past-only harmonic model. The prediction
is compared with actual samples only after they arrive. Unexpected withdrawal
of energy is treated as release rather than a new attack. Quiet new voices
can alternatively provide two novel independent spectral peaks. Existing
voices can continue or release outside admission. Per-frame note-strength
reassignment alone cannot reopen the interval. Quiet independent voices,
legato transitions and overlapping octave releases remain independent tests.

## Note lifecycle and integration

At most six generated voices are active. The polyphony cap also applies while
old voices are in release; newly admitted voices can retire absent weak voices
instead of creating an unbounded overlap. Amplitude informs MIDI velocity; all
Note Offs retain their original MIDI note. The correction engine subsequently
owns any pitch remapping, sustain and cross-source collision bookkeeping.

`configure` changing enable/mode/polyphony queues a reset for the next process
call. `reset` emits held Note Offs at the supplied offset, clears audio history
and status, and leaves settings intact. Disconnection, module bypass, transport
or restoration are handled by the wrapper, which also stops only the generated
source. An output-capacity fault is explicit; the wrapper must retire generated
bindings and reset the detector rather than silently losing a Note Off.

Processing does not allocate C++ heap objects, lock, build strings, read files
or call GUI code. Prepared harmonic tables are heap-owned, not a multi-megabyte
stack object. `Tests/PitchTests.cpp` checks functional audio conversion, declared
range, stable pure/bright detuning at three host rates, lifecycle and prepared
processing allocations. This is
separate from independent fixture validation and whole-host allocation tests.

## Evidence and scope

Early flexible least-squares estimation at 4–10 ms was compared against the same
recorded guitar attacks and rejected for inaccurate first notes. The experiment
is retained in `EarlyEstimator.md` and is not an enabled production fast path.
YIN and prior NSDF/correlation candidates were compared with independent frozen
fixtures. No test tolerance may be loosened to label this detector finished.

Neither a fast CPU kernel nor the plugin's reported compensation of zero
samples establishes acoustic/driver/host/receiver latency. Current measured
accuracy and latency, the release gate and any remaining limitations belong in
the release test report. Original pitch-method reference: de Cheveigne and
Kawahara (2002), YIN, DOI 10.1121/1.1458024. Implementations here are original
AGPL-3.0-or-later code; Buffkiller's audio filter is not used as a detector.
