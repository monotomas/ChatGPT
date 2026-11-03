# Independent pluginval host validation

The official Tracktion pluginval source was obtained through verified HTTPS Git
from https://github.com/Tracktion/pluginval, stable tag `v1.0.4`, commit
`ed19c2c16b57a6d94db391bea3ef4a80b769d5bf`. Its own JUCE submodule was kept at the
recorded commit `5179f4e720d8406ebd1b5401c86aea8da6cc83c9` (JUCE 7.0.2). Neither
source tree was patched. The validator is GPLv3 tooling, separate from Flysnapper.

It was built with GCC 14.2.0 against the existing isolated Debian sysroot. Its
LADSPA host header dependency was obtained from the signed Debian `ladspa-sdk`
package and checked against the signed-index SHA256 before extraction. Pluginval
was built with two jobs; this is a development tool, not an end-user dependency.

The initial native VST3 artifact (binary SHA256
`a9b5f7a179aa20908fcf343d625c80e09fbaf0498bd8bd0f9e874e3592bb243a`) passed:

| Level | Result | Reported groups | Wall time |
| --- | --- | ---: | ---: |
| 5, official minimum host-compatibility level | Passed, native exit 0 and SUCCESS | 19 | 2.26 s |
| 10, extended state restoration and parameter fuzzing | Passed, native exit 0 and SUCCESS | 25 | 13.87 s |

GUI tests were enabled. No tests were explicitly disabled. Default sample rates
44.1/48/96 kHz and block sizes 64/128/256/512/1024 were exercised. The seed was
`0x5eed1234`. Pluginval's optional external Steinberg validator integration was
reported as skipped because a standalone Steinberg validator path was not
provided. Non-VST3 format-specific branches are not claims of other-format tests.

These initial results predate later detector revisions and **must be rerun on the
final compiled artifact**. They validate plugin loading, callbacks, state,
automation, editor and bus stability. They do not establish Ableton behavior,
pitch accuracy or physical/algorithmic audio latency.

## Repeatable runner

`Tools/run_pluginval.py` checks both pinned source revisions, records the validator
binary and every plugin bundle member hash, captures the actual exit status and
completion marker, rejects zero-group runs, and verifies the plugin stayed
unchanged. It uses a fresh output directory and starts/stops its own local Xvfb
only when Linux has no existing DISPLAY. It does not create a web preview.

Example in the prepared native environment:

```bash
source Tools/cloud_env.sh
python Tools/run_pluginval.py \
  --tool /workspace/tools/pluginval-build/pluginval_artefacts/Release/pluginval \
  --source /workspace/tools/pluginval \
  --plugin build/native-release/Flysnapper_artefacts/Release/VST3/Flysnapper.vst3 \
  --output build/pluginval/strictness10-final --level 10
```

The source-built validator is retained under `/workspace/tools/pluginval` and its
build under `/workspace/tools/pluginval-build`. Its actual console and provenance
reports for the initial run are under `build/pluginval/`. The runner's protocol
regressions are `Tests/test_pluginval_runner.py` (four tests); those fixtures test
report interpretation and are not plugin validation themselves.
