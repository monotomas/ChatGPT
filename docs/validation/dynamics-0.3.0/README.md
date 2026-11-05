# Flysnapper0.3.0: scoped real-guitar evaluation evidence

The user's5November revision removes polyphonic bending, retains fixed-semitone
polyphony and mono bends, replaces absolute latency goals with optimization and
honest per-pitch reporting, and requests adjustable/learned input dynamics.

The final runtime passes all 15 native groups and86 bounded actual-processor cases.
The fresh [native checkpoint](native-attempt-4/README.md) includes raw MIDI,
calibration/resume/manual/failure/reset controls, all12quiet-mains plain/bend
cases and genuine octave transitions. Controlled first decisions are about
32msE2,24msA2,18.7msE3,16msA3,13.3msE4 and10.7msA4/E5 across9rate/block
configurations each. These are signal-decision times, not physical interface
or Ableton round-trip latency. Full min/median/max values and actual release
samples are in [results.json](results.json).

The broader [seen diagnostics](final-dsp-diagnostics/README.md) still have
substantial failures:98/209pass, compared with88/209on the paired earlier
source. All misses/extras/rearticulations/early and late offs remain reported.
They have not been turned into passes or hidden behind the bounded native
successes. This is evidence for preparing the requested real-guitar evaluation,
not a universal musical-readiness claim. Windows/installer/installed-VST3 proof
belongs to the separately collected delivery evidence.

The subsequent [qualified native run](native-qualified/README.md) repeats all
15 groups with the approved build-stage metadata and preserves the fresh
processor report, raw events and proof. Its runtime C++ inputs and module hash
match native4; the three changed builder/packaging scripts are identified
separately. This remains Linux evidence rather than Windows execution.

The architecture matters: muting idle noise before the existing stationary
observer prevented it from learning that noise; strong waveform compression
also biased a first MIDI anchor. Dynamics now follows the original noise
conditioning, with an independent cleaned frequency reference. An existing
plain-mode held-octave ambiguity was fixed with period-matched physical
observations, then falsified against genuine octave changes. Earlier failed
builds, source-bound captures and rejected alternatives remain archived here:

* [Initial external-dynamics topology](initial-topology-investigation/README.md)
* [Old mono compression ablations](mono-compressor-ablation-v1/README.md)
* [Exact detuned-repick failure](native-bend-diagnostic-v1/README.md)
* [Native2:13/15](native-attempt-2/README.md) and [native3:14/15](native-attempt-3/README.md)
* [Plain G1 also failing with dynamics off](plain-g1-bare-baseline/README.md)
* [Earlier integrated diagnostic](integrated-v2-diagnostics/README.md)
* [Rejected external-conditioner research](external-conditioner-research/README.md)

Every archive has an integrity inventory. The native2/3/4 snapshots deliberately
retain six selected canonical quiet-mains WAVs each because those files are
part of their 49-input fingerprints. All 18 copies match the unchanged sealed
originals byte for byte; they are not new recordings or independent data. Other
archives reference the original PCM, and runtime binaries are excluded from
these source/evidence archives. Calibration learns starting settings; manual control and its
noise-versus-tail tradeoff remain part of the intended real-guitar workflow.
