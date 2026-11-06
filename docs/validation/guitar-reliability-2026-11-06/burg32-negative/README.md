# Rejected causal Burg32 excitation predictor

This is a bounded negative experiment, not a product change or a claim that all autoregressive string models fail. Production source was untouched. The frozen musical baseline is `e235c8a098dadce6e0a4b8bd6ca9de789a5d174d`; the separate accepted nearby-fret change was not combined into this experiment.

The proposed independent excitation model fitted a maximum-order-32 Burg lattice to the last 512 received, noise-conditioned, pre-dynamics raw analysis samples. At the 12 kHz analysis rate, it forecast the next 32 samples open loop, then compared each prediction only when the corresponding input arrived. It operated only on held monophonic notes older than 70 ms. The error used `RAW / max(envelope, onsetEnvelope, gate)` normalization and the existing 0.5 innovation crossing. Frequency estimation, note admission, release rules and the polyphonic path were unchanged. Fixed-size arrays, stable reflection coefficients and finite/rank checks were used; no lookahead, parameter search or allocation was added. The fixed proposal and source hashes were saved before the first recording capture.

The mathematical checks were encouraging but insufficient: future RMS error was 0.125% for a sine, 6.28% for an eight-partial signal, 4.37% for a stiff inharmonic signal and 4.54% for a two-polarization synthetic string. On the selected real development asset the musical result was substantially worse:

| Check | Frozen baseline | Burg32 candidate |
| --- | ---: | ---: |
| DEV staccato nominal-52 asset, all eight mono profiles: note-ons | 25 | 40 |
| Additional nominal note-ons beyond one per profile | 17 | 32 |
| Quiet phase-only post-drop counter, exact two-note response | 14/16 | 8/16 |

The eight real-asset profiles comprise bend enabled/disabled, input dynamics default/disabled, and cold/350 ms idle startup. Source audio was 44.1 kHz; callback blocks were 64 samples. This was the same one preselected development asset used by the previous mono investigation. The author's SFZ maps this isolated asset to MIDI 52. The author does **not** document physical pick count, string contacts or damping events; the evidence does not establish those physical facts. No other development asset, reserved detector input, legacy-107 capture or broad-noise capture was run for this candidate after these first checks falsified it. Previously exposed reserved metadata remains disclosed in the sibling investigation; reserved detector runs remain zero.

The baseline quiet-counter 14/16 result already misses two E2 bend responses. This experiment must not be described as regressing a 16/16 baseline. Conversely, it lost six of the fourteen responses that were working. The legacy corpus's independently established baseline is 107/107 with dynamics disabled and 104/107 with default dynamics; neither was retested for this failed candidate.

## Causal failure trace

The instrumented cold/bend/dynamics-off replay reproduced all 140 events of the failed candidate exactly. A new false same-note cycle occurs at 162.653 ms, before the source's major 210–240 ms energy withdrawal. The first new innovation arm is at 149.161 ms. The following measurements describe the 32-sample forecast hop containing that arm:

| Quantity | Value |
| --- | ---: |
| Used Burg degree | 32 |
| Training-window input RMS | 0.0726802 |
| Future-hop actual input RMS | 0.0736923 |
| Fitted one-step residual / training input RMS | 0.0378992 |
| Open-loop forecast error / training input RMS | 0.9689173 |
| Original one-period predictor error / same training input RMS | 0.0732192 |
| AR maximum absolute error / period-aware instantaneous denominator | 2.02026 |
| Original predictor maximum error / same denominator | 0.189842 |
| Maximum smoothed innovation ratio | 0.538685 |

The common RMS reference matters: low-energy portions of other hops inflate a future-hop-relative error. Both normalizations are preserved explicitly in `diagnostic/run-rms-reference/common-rms-reference.json`; at this first false-arm hop the two input RMS values are almost equal. A small fitted residual therefore did not translate into a reliable causal open-loop forecast. Review of index alignment and stable lattice synthesis found no evident finite-value or one-sample alignment defect. This is not proof that the model class is impossible or that all implementation defects have been excluded.

The proposed explanation that high partials exceed the AR32 model's capacity was not supported by this trace. The training windows contained only 0.024–0.085% power above sixteen times the independently tracked fundamental, and about 1.01% of the forecast error's spectral energy was above that boundary. Most failure energy was in the lower band. An unconstrained AR32 does not allocate its modes directly to the first sixteen harmonics, so this diagnostic does not prove an AR64 model could never help. It does falsify the particular claim that omitted upper-band energy dominates this failure. No order grid or AR64 follow-up was run.

Observed maximum callback duration was 410–968 microseconds across the eight candidate captures, with zero measured deadline misses. These are shared-machine observations, not a real-time CPU certificate. Musical failures alone reject the candidate.

## Preserved evidence and reproduction

`storage-map.json` maps every logical private-study path to a deduplicated payload. CSV payloads use deterministic, lossless gzip; other payloads are stored verbatim. Source, drivers, raw MIDI events, forecast-error rows, derived power spectra, pre-capture declarations and actual compile bindings are included. Five executable files are excluded by content signature and separately inventoried; no audio, float PCM, object, model or installer payload is stored. The spectra are derived measurements, not serialized audio or model state.

The original candidate is `Source/`; the instrumented diagnostic copy is `diagnostic/Source/`. The latter adds diagnostic fields and trace storage. Its complete source must not be mistaken for the original before-capture snapshot. The first diagnostic capture's compile command is reconstructed in `diagnostic-first-capture-provenance.json` and explicitly labelled retrospective; the second diagnostic capture and both original musical checks have saved actual commands and executable hashes. Runtime launch commands reconstructed from the retained drivers are likewise labelled retrospective.

Run `python3 verify_restore.py --output /tmp/NEW_EMPTY_LOCATION` to validate every archived file, restore all logical study files and check saved source bindings. Run `python3 replay_quiet_counter.py /tmp/NEW_EMPTY_LOCATION /tmp/ANOTHER_EMPTY_LOCATION` to build a new executable from the restored original candidate and reproduce both counter CSVs byte for byte. A completed independent restore/rebuild check is recorded in `restoration-rebuild-check.json`. Real-asset reproduction additionally requires the separately licensed input with the SHA-256 in `external-input.json`; it is intentionally not duplicated here.

For the original baseline's 12,474-event parity, 558-event real-asset parity, lifecycle/RAW/normalization failures and the complete baseline source, see the sibling [mono-trigger investigation](../mono-trigger-investigation/README.md). None of those rejected patches is adopted here.
