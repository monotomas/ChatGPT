# Three bounded parameter ablations of the unchanged mono repick test

These private test variants alter only declared dynamics parameter settings in
the test harness. The production source, +30-cent waveform and assertions are
unchanged. They were compiled against the exact earlier processor header/ABI
retained in `../native-bend-diagnostic-v1/source/`, and linked against the still
retained old-topology native shared objects under the common build lock.
Before/after hashes establish those linked objects did not change. Binaries and
object files are deliberately excluded here.

| Parameter variant | Entire old BendProcessor result |
| --- | --- |
| Dynamics disabled | PASS477697 checks |
| Default gate, compressor ratio1/threshold0/ceiling0 | PASS477775 checks |
| Default compressor, gate threshold-96/hysteresis0 | FAIL: same initial anchor58 instead of57 |

The compression-only failure has the same actual44.1kHz/range2 Note Ons58 at
705,14935,29047 and final Off47980. Full MIDI bytes are retained. This isolates
the new semitone-anchor bias to compression, separately from the quiet-hum
regressions isolated to gating in the broader capture ablations. It does not
justify disabling waveform/voice assertions or claiming every compressor
setting is safe. Exactly these three variants were run; no threshold search
was performed. A later accepted topology/reference fix needs its own fresh
complete native suite and measured outputs.

`manifest.json` binds every private source variant, resulting executable hash
and all linked object/library hashes. Compiler/link stdout and complete test
console output are preserved. The source variants are measured experiments,
not modifications copied into production or a new acceptance-policy edition.
