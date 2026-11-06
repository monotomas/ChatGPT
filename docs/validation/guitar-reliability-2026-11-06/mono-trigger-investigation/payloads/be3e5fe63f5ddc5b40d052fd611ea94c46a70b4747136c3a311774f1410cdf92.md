# Sealed independent holdout

Prepared on 3 November 2026 before the development algorithm passed the fixed
30-case quality gate. Do not use this corpus for tuning before recording an
algorithm/source freeze. Its first evaluation is a generalization audit, not a
new adjustable acceptance gate. The existing development 81 cases and fixed 30-case
policy have not been changed.

There are 39 PCM waveforms and 69 case/mode combinations. They cover independently
seeded plucked modal strings with different pluck positions, partial balances,
attack times, slight detuning and spectral holes, nine new polyphonic mixtures,
and twelve edited guitar recordings from two additional timbres. The notes and
source recording labels were checked with sustained harmonic peak frequencies;
that provenance is retained in fixtures.json. Frequency labels are not inferred
from the detector under test.

## Recorded material and attribution

Original recordings are copied byte for byte from Nicholaus P. Brosowsky's
[tonejs-instruments](https://github.com/nbrosowsky/tonejs-instruments), pinned to
commit `622c2f1c32c8cfce4158ddc3eb26e518ddef37e5`.
The upstream samples are licensed under
[CC-BY-3.0](https://creativecommons.org/licenses/by/3.0/).
`tonejs/manifest.json` records every original source path and SHA256. The upstream
README, LICENSE.md and sample-source-info.txt are retained.

Attribution: electric guitar source by Karoryfer Samples
(<https://www.karoryfer.com/karoryfer-samples>); nylon/classical guitar source by
Freesound contributor quartertone, "classicalguitar-multisampled" (11573);
collection and sample editing by Nicholaus P. Brosowsky. The upstream collection
states that samples have been trimmed, ramped, normalized, denoised and sometimes
pitch-corrected. These recordings are edited isolated notes, not a live
performance corpus.

Our derived recordings are decoded to mono 48kHz, multiplied by .35, cropped to
650ms from the original file start (including the original attack/leading
silence), padded with 50ms initial silence and 250ms terminal silence, with a 4ms
terminal taper. Reference onset uses the same fixed absolute amplitude .001
(-60dBFS) rule as the main corpus. Derived recordings remain CC-BY-3.0; these
modifications and authorship are declared here. Original Ogg files are unchanged.

The original synthetic waveforms and annotations are AGPL-3.0-or-later with the
Flysnapper source. They are not asserted to be real guitar recordings.

## Integrity and execution

`python Tools/audio_validation_holdout.py --verify` uses only the Python standard
library to check all original and derived hashes, audio parameters and reference
inventory. Regeneration without `--verify` requires numpy and FFmpeg.

After freezing the candidate algorithm:

```sh
python Tools/audio_validation_run.py --holdout --output docs/validation/audio-holdout
python Tools/audio_validation_run.py --holdout --full --output docs/validation/audio-holdout-full
```

This uses the same fixed scoring rules as the main measurement corpus. It does
not automatically change the 30-case quality policy or certify physical 5/10ms
latency. Any failed observations must remain visible in the final accuracy and
response report.
