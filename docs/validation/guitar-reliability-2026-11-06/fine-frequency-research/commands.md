# Retained commands and reproduction scope

The original exact Python command construction is retained in:

- `preserved/run_frequency_experiment.py`: standalone O2/DNDEBUG object builds,
  timed warm drivers and330/132 runs for fixed/median variants.
- `preserved/run_uncertainty_experiment.py`: uncertainty source and warm runs.
- `preserved/run_cold_frequency_experiment.py`: fresh-detector cold drivers,
  links against recorded object hashes and all six cold captures.
- `preserved/profile_thread_cpu.py` and `_cold.py`: exact source instrumentation,
  standalone builds, case10 selectors and warm/cold profile captures.
- `preserved/score_frequency_experiment.py`: unchanged chord mappings and causal
  interval definitions. The original reports remain byte-for-byte preserved.

Original manifests retain actual source, driver, object/executable and output
hashes. Original matrix/diagnostic compile logs contain no text; an exact original
argv was not recorded there. The following reconstructs the retained matrix audit
with the same standalone flags and complete archived source; it is a reproduction
command, not a fabricated historical command log:

```sh
archive_root=/workspace/ChatGPT/docs/validation/guitar-reliability-2026-11-06/fine-frequency-research
scratch_output=/tmp/new-f0-matrix-audit
mkdir "$scratch_output"
g++ -std=c++17 -O2 -DNDEBUG \
  -I "$archive_root/preserved/observed-f0/Source" \
  -I "$archive_root/preserved/observed-f0" \
  "$archive_root/preserved/gram_audit.cpp" -o "$scratch_output/gram_audit"
"$scratch_output/gram_audit"
```

For detector capture, `reproduce.py` restores full exact source and records its
actual compile/run argv in a new manifest. It was executed for30 uncertainty
open-string configurations; raw event bytes match the original cold330 subset.
Both timings and executable hashes are machine-dependent; no installer or shared
native CMake/Ninja build is involved.

The separate lifecycle `run.py` records the same standalone eight-translation-unit
object build and four unchanged330/132 warm/cold drivers. Its first attempt stopped
after successful compilation because the scorer's `__file__` was missing in the
wrapper namespace. That wrapper error log is retained; the corrected runner
recompiled all objects before capture. No detector source or scorer policy was
changed to repair this harness error.
