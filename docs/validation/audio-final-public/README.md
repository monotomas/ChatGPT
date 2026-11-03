# Final source 19 — native public-corpus evidence

These five evidence files are copied **unchanged** from the native Linux
`FlysnapperAudioQuality` CTest run at 2026-11-03T20:34:48Z. Their source hashes
match frozen source 19, and their summary/metrics hashes match
`report-manifest.json`. The release evaluator SHA256 is
`7541c5e8ef174e34bb97f06de4b69bb4a374ad6650d7b0758f19ec0fb0417135`.

`metrics.csv` and `events.csv` cover all 81 public development cases at
48 kHz / 64 samples. Mono matches 50/50 reference notes with no counted errors.
Poly matches 64/67 with three misses, one extra octave pitch, six
wrong-before-correct counts and one early release; no voice hangs or overflow.
The entire public event file is independently verified byte-identical to18.

The separate **30-case basic gate passes**. This does not make the other public
poly errors disappear or certify physical live latency. These are development
cases already used for algorithm work. The two final disclosed-corpus reports
are retained alongside this directory; original blind 14/16 evidence is preserved.

This Linux evidence does not replace actual Windows/MSVC, installed-module,
Ableton or driver tests. Consult the specific Windows package's `Validation`
files for its native results. CPU measurements remain untrimmed and separate
from detector response time and musical accuracy.

The `audio-cases.csv`, `audio-events.csv`, `audio-summary.txt` and
`measurement-manifest.json` files record a separate final rerun of that same
native evaluator at 20:39 UTC. Its events are byte-identical; CPU timings belong
to that separate execution. The evaluator hash is identical and source stability
is verified. Neither run is a new unseen-data audit.
