# Mono trigger failures and rejected real-pluck repairs

These two complete investigations use the exact 0.3.0 base
`e235c8a098dadce6e0a4b8bd6ca9de789a5d174d`. **No trigger patch here is approved.**
The root's separate nearby-fret correction and current native test results live
outside this archive. This evidence is not an installer or musical acceptance.

Read the [initial mono audit](INITIAL-MONO_REPORT.md) and
[real-repeat investigation](REAL-REPEAT_REPORT.md). They retain the failures,
negative controls and limits instead of converting aggregate improvements into
passes.

* The initial baseline reproduces all **12,474 events across 106 cases** exactly,
  including zero-event cases. Plain/bend coverage reveals fast-fret omissions,
  same-sample Off/On repeats and release/reacquisition gaps.
* The new real DEV staccato asset reproduces all **558 events across eight
  profiles** exactly. An author SFZ isolated sample does **not** establish a
  documented physical pick count, per-recording contact time or damping truth.
* The complete legacy comparison retains the distinct **107/107 dynamics OFF**
  and **104/107 DEFAULT** baseline. The three DEFAULT omissions are final
  repicks, whereas a separate rejected unified-phase change loses middle
  repicks with dynamics OFF. Those failures must not be conflated.
* The new post-withdrawal quiet-phase counter has **14/16 baseline successes**,
  not 16/16. Lifecycle vetoes remove the 17 real-DEV repeats but lose genuine
  quiet excitations. Raw-innovation/floor candidates recover DEFAULT notes yet
  introduce other false triggers. Phase-variance, flag, predictor-period,
  lifecycle and normalization results all remain rejected.
* Seven prospective reserved **detector inputs remain unrun**. Their descriptive
  annotation metadata was visible to root and this agent; they are not described
  as fully hidden metadata or a new independent instrument. No reserved detector
  output is included.

## Preservation

`storage-map.json` binds **1,347 original logical files** to **581 unique
payloads**. CSV is losslessly gzip-compressed with deterministic headers;
identical bytes share one payload. All source, raw MIDI/trace/scoring output,
compiler logs, scripts, source-bound manifests and negative-counter evidence
are retained. No quantization or reformatting changes their original bytes.

`canonical/e235/Source` preserves the complete tracked base Source tree directly
from Git. `source-snapshots.json` binds **26 complete source snapshots**, including
trace-only access/fields where present. Restore reconstructs every original
source path and file; each hash is checked. This is project-source preservation,
not a complete compiler/JUCE/system-header dependency closure.

`runtime-bindings.json` links each of the **37 excluded ELF executables** to its
retained driver and source snapshot and records its SHA/size without copying
its binary. Original DEV before-compile and measurement manifests remain exact.
Some earlier compiles were issued inline during the session; their replay
commands and final source/driver bindings are explicitly **retrospective**.
Where a replay source order is reconstructed as sorted, it is not falsely
claimed to be the original recorded command. No attestation is backdated.

The first mono runner later gained argv/output-name conveniences; its earlier
version is explicitly inverse-reconstructed with the same retrospective caveat
in initial-runner-reconstruction.json. The scoring body/evaluator is unchanged.

The first real trace was debug-extended in place for trace v2. Its inverse v1
source reconstruction is separately labelled in `trace-v1-reconstruction.json`.
Original v1 before-compile hashes and overwritten executable were not retained;
the original v1/v2 raw streams and their separate 558-event baseline equality
remain available. Reconstructed source is not presented as a contemporaneous
hash attestation.

No ELF, PE, object/library, model/weight, PCM or decoded f32 payload is stored.
`exclusions.json` inventories omitted runtime hashes. Shared fixture seals,
waveform hashes and legal/DEV partition ownership remain in canonical fixture
metadata; audio is supplied separately from those unchanged sources.

## Verify and restore

    python verify_restore.py
    python verify_restore.py --project-root /workspace/ChatGPT
    python verify_restore.py --output /tmp/new-empty-trigger-study

The optional project-root check reads metadata/licenses only. Default verification
and restoration never load or execute audio, runtime modules or models. A restore
must use a new directory outside the archive and is byte-for-byte lossless.

The restored `studies/initial-mono/` and `studies/real-repeat/` trees preserve
original logical paths. Their historical manifests keep real absolute paths.
For a new capture, pass the reconstructed source/driver paths and unchanged
canonical inputs to a fresh output directory, recording a new manifest. Do not
rewrite an old record's paths, measurements or source hashes.

`replay_quiet_counter.py` optionally compiles one restored rejected candidate and
compares both deterministic counter CSVs exactly. It needs g++, reads no external
recording, and is an archive reproduction check rather than product acceptance.
The README's counts describe the frozen inventory; later root-level native,
Windows, interface and actual-guitar checks are separate evidence.
