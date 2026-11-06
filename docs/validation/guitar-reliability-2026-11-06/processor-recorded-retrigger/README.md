# Actual-processor original-pluck fragmentation regression

This is a failing development regression, not a new plugin release. The real
processor receives the original CC0 development sample through its sidechain,
with no sample gain, resampling or normalization. No reserved sample is run.
The exact source/test/input hashes are identical before and after the run.
Runtime Source matches research commit `1dcdbb6f5fe50b36f890e1dd37e13729c5ea9b61`;
only the new test differs. The frozen test and incremental build log are retained.
These are selected project inputs, not a full compiler/system-header closure.
The binary is excluded but identified by SHA256 in `result.json`.

Original `staccato_e4_rr3.wav` has author MIDI key center52,44100Hz monoPCM24,
25169 frames and SHA256 `8c9d3d8a8a9a1880d44c83d4f966bd0de0139b685df244e7b4ac8e4afe8108f8`.
The original uses WAVE_FORMAT_EXTENSIBLE with a PCM GUID. The reader checks that
subformat and decodes signed24-bit samples exactly. The original fixture edition
verifier independently checks all bytes. Its immutable input is referenced, not
copied here. The first draft reader rejected the extensible wrapper before
processing; that test-development failure was corrected before this retained run.
It supplies no musical evidence.

All88 preceding processor cases pass. The eight new profiles all fail, emitting
4/3/2/3/4/3/3/3 Note Ons instead of one (plain/bend, dynamics off/default,
0/350ms leading silence). Each Off is balanced and no voices remain active.
The executable exits1 after preserving all eight failed cases. The previously
failing six-open-string test follows and is not reached in this run; its earlier
failure is not repaired or replaced. There is no repeated full native run or
installer. The new regression remains mandatory.

An authored isolated sample is not a continuous human technique performance.
File start, acoustic activity and file end do not give physical pick/damping
truth. One-note fragmentation is an explicit regression expectation for this
asset, not an externally annotated physical count of all microscopic contacts.
File-relative decision samples must not be relabelled as acoustic latency.

`run.py` is the exact local command recorder with absolute paths; it creates a
temporary Xvfb, holds the native lock, runs the built processor executable and
records source/input hashes. It is not a universal setup script. Build the frozen
test against the source above using `Tools/cloud_env.sh` and the shared native
lock; then run that recorder only in its documented local build directory.
