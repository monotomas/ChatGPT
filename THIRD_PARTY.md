# Third-party notices

Flysnapper 0.1.0 is a routing/MIDI prototype. The complete GNU Affero General
Public License v3 is in `LICENSE.txt`. Project sources use AGPL-3.0-or-later.

## Buffkiller reference

Chord data/recognition principles, visual palette and Windows builder utilities
are reused/adapted from the supplied Buffkiller 1.0.0 source package, authored
for Olav Lægreid. Original SPDX notices are retained in reused source files.
The Buffkiller product and original reference archive are unchanged.
`build_plugin.py` and its discovery/Unicode regression fixtures adapt the
reference's checked MSVC/Windows SDK tooling and Unicode build-tool correction.

## JUCE and bundled VST3 SDK

- Official source: https://github.com/juce-framework/JUCE
- Tag: `8.0.16-security`
- Required commit: `ea77444750de45e036e0279bb0291144bf8e3688`
- Copyright: Raw Material Software Limited and contributors identified upstream.
- This build selects the **AGPLv3 option**, not a commercial JUCE licence.

The builder retrieves a clean checkout into `deps/JUCE` or verifies an explicitly
supplied `--juce-dir`. It rejects modified sources. JUCE's `LICENSE.md`, module
notices and bundled third-party licences remain authoritative and must be retained.
JUCE includes the VST3 support/SDK needed here, including its upstream notices.

On Windows only, the builder creates a content-verified private JUCE copy and
patches `extras/Build/juceaide/Main.cpp` to accept wide Windows arguments and
preserve diagnostic errors. The clean dependency checkout is never patched.
The exact patch is visible in `build_plugin.py`; an actual Unicode resource
probe runs before the plugin build. Python tests exercise fixtures; they do
not claim MSVC or Windows execution when run on Linux.

VST is a trademark of Steinberg Media Technologies GmbH. Flysnapper is not
endorsed by Steinberg, Ableton, Microsoft or JUCE. Before redistributing a
compiled binary, provide corresponding source and notices as required by the
selected licences. No closed-source commercial JUCE licence is granted here.

## External tools and assets

CMake, Git, Python, MSVC and the Windows SDK are external build tools. No external
font, sample, model, impulse response or artwork is embedded. No audio pitch
estimation algorithm is shipped in this routing prototype.

## Guitar validation recordings

Eight acoustic guitar notes from Nicholaus P. Brosowsky's tonejs-instruments,
commit 622c2f1c32c8cfce4158ddc3eb26e518ddef37e5, are included under the upstream
CC-BY 3.0 sample license. Original source: University of Iowa Electronic Music
Studios acoustic guitar sample library. Original compressed recordings and
upstream attribution/license files are retained in Tests/AudioFixtures/tonejs.
Decoded/cropped/gain-adjusted validation WAVs remain CC-BY 3.0. Exact changes,
source links and hashes: Tests/AudioFixtures/README.md and tonejs/manifest.json.
These are test data; no samples or upstream detector code are used in plugin
processing. Synthetic test waveforms are AGPL-3.0-or-later project data.
