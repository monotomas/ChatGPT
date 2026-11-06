# V4 mono latency: bounded nearby-fret candidate and rejected shortcuts

This is private development evidence against source
`e235c8a098dadce6e0a4b8bd6ca9de789a5d174d`, using disclosed regression data.
It is not a release, physical-guitar validation or general musical acceptance.
Production Source was not edited by this investigation. The parent integrates
any reviewed candidate separately and must measure the final combined source.

## Candidate retained for review

`local_secondary128` preserves the original global512-sample mono estimator
for acquisition, steady same-MIDI hypotheses, invalid same-note continuation,
distant/octave transitions and the entire bend-mode path. A separate past-only
3-period observation (minimum128, the physical helper's existing domain)
can propose a different nearby MIDI fret within six semitones. It requires a
resolved physical root plus at least three partials, then uses the existing
admissibility, note history and output logic. A local hypothesis agreeing with
the newly held note may protect it from a stale different nearby global result;
invalid global plus unchanged local/held identity cannot bypass release.

The exact patch is `orchestration/local-secondary128.patch`. The candidate
PitchDetector.cpp SHA256 is
`efd2af21e0c2cda9266977ba453363ba1e4c647ed8ae949665c31e1d52efc96d`.
All initial-note latencies remain unchanged. The extra bounded estimate costs
CPU even when ignored; shared-worker timing must not be called a certified bound.

* Plain noncontinuous controls:62/74→74/74 useful references; extras24→18,
  octave extras12 unchanged, early offs6 unchanged. Both35ms fret runs become
  8/8 notes instead of2/8 each; interior decisions are about15–22ms.
* All bend-mode event streams across the three corpora are byte-identical.
  Plain recorded-noise events are byte-identical; hammer note outcomes are
  unchanged, with some corrective-event timing changes.
* The seven predeclared real DEV plucks produce byte-identical56-run event
  streams, including the existing wrong/repeated notes. Reserved data ran0times.
* All32 genuine octave/step/damp countercases (64 references) and24 quiet
  configurations retain correct event order/lifetimes. Quiet streams and all
  genuine octave timings are byte-identical. Only the plain52→54 step improves
  from47.3125 to20.6458ms; its return timing stays27.9792ms.
* All88 legacy mono cases (107 references) across public/holdout/confirmation
  retain byte-identical events, separately with dynamics off and default.
  Off finds107/107 with two preexisting extras; default finds104/107 with one
  preexisting extra. Default misses one reference each in
  `continuous_repick_52_mono`, `confirm_articulation_02_mono` and
  `confirm_articulation_03_mono`. These remain failures.

The original continuous-bend truth is retained for plain captures, explicitly
marked as a diagnostic output-requirement mismatch. Those three cases are
excluded only from the stated **noncontinuous** plain comparison, never counted
as passes. Every raw capture and its original scoring/failure remain retained.
Legacy107 uses the unchanged independent20ms useful-note matching and numerical
tolerances. The older bounded counter script's80ms onset check is a retained
mechanism assertion, not a new user latency acceptance gate; actual timestamps
and every transition delay remain available.

## Rejected approaches remain evidence

`overlap15` and `fresh15b` lower the fresh YIN observation requirement and
quietE2 response (~32→25.3ms), but introduce real recorded-noise octave errors
and more incorrect attack anchors. They are rejected, not combined.

`adaptive_held` globally gives plain mode the existing bend-mode3-period
window and restores all fret notes, but slows genuine downward octaves and
changes real default-dynamics repeats3→4. Wider-root fallback variants either
invent an intermediate fret or retain timing regressions. These are rejected.
The historical work log preserves the order of experiments; pending labels
in that earlier log are superseded by this final report and actual results.

## Preservation and reproduction

`storage-map.json` binds558 original logical files to losslessly stored payloads.
CSV files are gzip-compressed with deterministic metadata. Identical payloads
are deduplicated; several logical files may point to one stored file. No PCM,
runtime executable, object file, external model or weight is included. Canonical
fixture seals and recording hashes remain in their original manifests.

`source/baseline/Source` preserves the complete29-file Source snapshot used as
the base. Each variant stores only its changed Source files plus the original
full source hash map. Reconstruction overlays that variant on the base and
verifies all29 hashes. This is project-source binding, not a full compiler/JUCE/
system-header dependency closure. Direct detector builds exclude MpeOutputRouter
and use the exact eight Pitch translation units named in their manifests.

The real DEV benchmark's baseline and exact driver/runner are retained here;
original CC0 recordings and DEV/RESERVED selection ownership remain with the
separate real-benchmark provenance. Do not use the source filenames or timing
as claims about physical pick contact, physical damping or continuous technique.

Run `python verify_restore.py` to verify every retained payload and reconstructed
source hash without writing files. `--output /tmp/new-empty-directory` restores
the original logical text/CSV files and complete source variants. Supply the
unchanged canonical fixture/DEV sources through the documented driver arguments.
Original capture manifests retain their real historical absolute paths; a later
capture needs a new manifest, not edits to these records.
