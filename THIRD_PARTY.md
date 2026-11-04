# Third-party notices

Flysnapper 0.2.1 contains audio-to-MIDI detection and MIDI correction. The complete
GNU Affero General Public License v3 is in `LICENSE.txt`. Project sources use
AGPL-3.0-or-later. A Windows installation package includes corresponding project
source under `Source/Flysnapper-Source.zip` and a lock for the JUCE dependency.

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

CMake, Git, Python, MSVC and the Windows SDK are external build tools. Optional
pluginval validation uses the separately built official Tracktion tool; it is
not part of the plugin. Upstream tool and JUCE notices remain authoritative.

The pitch detector is implemented in the project's `Source/Pitch` directory.
Its YIN estimator follows the published cumulative mean-normalised difference
method of Alain de Cheveigné and Hideki Kawahara, “YIN, a fundamental frequency
estimator for speech and music”, JASA 111 (2002), DOI 10.1121/1.1458024. This is
an independent implementation, not copied third-party estimator code. Harmonic
group pursuit, note lifecycle and integration code are also included as source.
No pretrained model or commercial pitch engine is embedded.

## Guitar validation recordings

The development, first holdout and second confirmation corpora include guitar
recordings from Nicholaus P. Brosowsky's **tonejs-instruments**, pinned to commit
`622c2f1c32c8cfce4158ddc3eb26e518ddef37e5`:

- Collection: https://github.com/nbrosowsky/tonejs-instruments
- Sample licence stated by the collection: Creative Commons Attribution 3.0,
  https://creativecommons.org/licenses/by/3.0/
- Acoustic guitar source: University of Iowa Electronic Music Studios,
  http://theremin.music.uiowa.edu/
- Electric guitar source: Karoryfer Samples,
  https://www.karoryfer.com/karoryfer-samples
- Nylon guitar source: Freesound user **quartertone**, pack
  `11573__quartertone__classicalguitar-multisampled`, https://freesound.org/

The original compressed recordings, upstream licence/readme/source-attribution
files and per-file hashes are retained in the `tonejs` directories under
`Tests/AudioFixtures`, `Tests/AudioFixtures/holdout` and
`Tests/AudioFixtures/confirmation`. Their respective READMEs and manifests
identify the active recordings, exclusions and exact derivation steps. Derived
WAVs apply the documented decoding, mono conversion, gain, crop, terminal taper
and silence padding; they remain under CC-BY 3.0. Upstream trimming, normalisation,
denoising and occasional pitch correction are described in the retained notices.
Artificial mixtures of isolated recordings are identified as such.

These samples are test data included in corresponding source. The plugin does
not play, embed or learn from these recordings at runtime. Synthetic test
waveforms and project fixture-generation tools use AGPL-3.0-or-later.

## Additional research guitar recordings

The unfinished musical-readiness research also retains recordings from two
CC0-1.0 Karoryfer libraries:

- **Black and Green Guitars**, Brian Wood / Karoryfer Lecolds; Green Gretsch
  Anniversary. Repository `sfzinstruments/karoryfer.black-and-green-guitars`,
  commit `b3b3249d37dc977a1a297bd2dc053e6d9b6b805c`.
- **Shinyguitar**, performed by D. Smolken. Repository
  `sfzinstruments/karoryfer.shinyguitar`, commit
  `57243cca85277dbcc120ce17c6178032f93c80f3`.

Original licenses, author SFZ mappings, file hashes and derivation notes are
retained under `Tests/AudioFixtures/readiness/recorded_hammers/source` and
`recorded_noise/source`. Derived controls use documented decoding/resampling,
gain, crop, damping and mixtures. Author-labelled percussion/muting samples
are designated suppression controls, not proof that every such waveform is
non-tonal or unwanted. Isolated hammer samples do not establish continuous
fretted-transition recognition. These recordings are research data; they are
not embedded or used as pretrained weights in the plugin.
