# Plain low-note octave ambiguity: Native3 regression and correction

This archive preserves a real failed native integration check and the bounded
correction. It does not replace the final native/Windows/installer evidence or
claim that every possible octave/missing-fundamental spectrum is recoverable.

## Failure and diagnosis

Native3 passed 14/15 test groups. The first failing quiet control was ordinary
mono G1 with bending disabled, even though the previous bend-enabled broad
captures were clean. It emitted G1 at sample 26367, then replaced it with G2 at
40191, more than 700 ms before the wanted G1's 74400 cutoff.

An independent exact-PCM comparison demonstrates the same old plain-mode
failure with dynamics **disabled**. It is not a new compressor/gate-only
regression. Of 24 combinations (six sealed 31/40/52 gain 1/8 waveforms, plain/bend,
dynamics off/on), only four plain 31 cases failed. The complete original
[baseline report](REPORT.md), raw events, per-block status and PCM/source hashes
are unchanged here.

At the failure, actual dynamics gain is 0 dB and raw/processed frames agree.
The held 49 Hz tracker has correlation above 0.996. A 512-sample physical-series
check spans barely two G1 periods and sees only the strong even partials
(mask 682); the longer already received history retains a physical root and odd
partials. The source has changed timbre, not moved to a higher fret.

## Adopted correction and falsification

Only the physical-evidence checks for a held octave ambiguity change. They use
the cleaned reference before dynamics gain, with at least the existing 512
samples and the tracker's existing 3.25-period observation when longer, capped
by the available 1024 samples. No threshold is lowered and periodicity alone
does not latch an old note. Genuine root/odd-partial withdrawal must still let
the octave change through. Ordinary guitar pitches 40+ retain the 512-sample
minimum window.

The [candidate report](raw-physical-candidate-validation/REPORT.md) verifies:

* All 24 quiet combinations now have exactly one correct balanced note, no
  unwanted extra note, and no early/late release under the original limits.
  Only the four failed plain 31 streams change; the other 20 complete raw
  streams are byte-identical. G1 release is sample 76671, 47.31 ms after cutoff.
* A fixed predeclared 32-case counterset tests roots 31/35/40/52, both plain and
  bend modes, true octave up/down, discrete two-semitone steps and physical
  damping, with dynamics enabled. All 64 intended references are found with
  no extra notes, early/late release, overflow or hanging voice.
* Every counter event, reference decision, status and metric CSV is
  byte-identical to baseline. Actual decision delays are 13.31–63.31 ms and
  release delays 17.98–60.65 ms; this correction adds no measured delay in that
  counterset. It adds no future-audio buffer: the reference history was
  already received.
* Existing Pitch 112,894 and GuitarQuality 93,967 checks pass on the frozen
  candidate, including genuine octave changes, bend controls, repicks, range,
  stable detuning, sample offsets and allocation checks. Their manifests and
  logs are under `functional/`.

These are deterministic controls at 48 kHz/64 samples, not physical performer
latency or universal guitar coverage. The earlier 15 cold near-mains detuning
misses remain explicitly outside this integer-pitch counterset; they were
not silently repaired or omitted from the earlier 876-case report.

## Retention

Original source snapshots, exact candidate patch, predeclared generator,
complete raw counters and historical manifests are preserved. Compiled
executables and duplicate PCM are excluded; their hashes are recorded where
available. `retention.json` explains the original temporary paths and the small
main-code cleanup that avoids an unused physical projection on the upward
branch. Actual native processor validation binds the final adopted source.

The top `archive-manifest.json` hashes every retained text record. Subdirectory
manifests are retained byte-for-byte and continue to verify their original
listed scope. Reproduction uses fresh output directories and original sealed
PCM, never altered labels or later rescored timestamps.
