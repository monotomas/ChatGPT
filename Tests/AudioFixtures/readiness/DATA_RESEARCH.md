# Recorded-performance source research

Recorded evidence must carry its own provenance and annotations. A rendered
transcription, filename alone, or a dummy loader fixture is insufficient.
The retained CC0 `recorded_hammers` and `recorded_noise` editions document their
precise limits; neither closes the continuous real-articulation requirement.

## Official Guitar-TECHS dependency

Primary site: <https://github.com/guitar-techs/guitar-techs.github.io>, pinned at
`19a2954f789bfd192b2b4732788ceb000c1dd687`, `index.html`.
Dataset: <https://zenodo.org/records/14963133>. The site expressly licenses
**all data CC BY 4.0**. Credit Hegel Pedroza, Wallace Abreu, Ryan M. Corey and
Iran R. Roman, “Guitar-TECHS: An Electric Guitar Dataset Covering Techniques,
Musical Excerpts, Chords and Scales Using a Diverse Array of Hardware”, ICASSP
2025. The GitHub tree contains the site and images, not raw audio or annotations.

The source describes three professional players with different guitars,
rooms/interfaces and captures, including DI, amplified microphone and two stereo
perspectives. Named technique classes are single notes, palm mute, vibrato,
pinch harmonics, harmonics and bendings. Continuous hammer/pull/fretted-slide
classes are not individually documented; musical excerpts/scales need inspection
and independent annotation before making those claims.

Its synchronized MIDI labels were automatically captured by a Fishman Triple
Play Connect pickup. They are valuable independent performance references but
are not human-labelled acoustic-onset or fractional-pitch truth. Pickup MIDI
delay/errors must not be mistaken for physical latency ground truth for
Flysnapper's original E2-under-10-ms/E3-under-5-ms targets.

The enforced cloud HTTP policy denies `zenodo.org` (CONNECT 403). A supported
additive environment configuration draft has been saved; that does not apply
runtime policy until the user publishes/saves the settings. No proxy, alternate
route or Actions/cache bypass was attempted. Exact Zenodo archive filenames,
hashes and per-clip annotations remain **unknown**, rather than guessed from a
paper. Once actually permitted, retrieve the record metadata first, pin its
file checksums, then select the smallest DI/technique+annotation subset before
any new detector evaluation.

## Additional GitHub sources inspected

- `sfzinstruments/karoryfer.shinyguitar`, commit
  `57243cca85277dbcc120ce17c6178032f93c80f3`: CC0, genuine plucks and labelled
  muted/percussive gestures retained in `recorded_noise`. Complete file tree has
  no separately named recorded slide/bend/continuous-legato files. Its MIDI-CC
  vibrato settings are sampler modulation, not recorded human-vibrato evidence.
- `Weixin8999/GapTec`, commit `c8ace38da37b64388525ef85af9edc1844524c4d`:
  contains technique WAV directories including bending/trill. README says its
  6,580 clips were downloaded from the Su/Yu/Yang GuitarTranscription database,
  <http://mac.citi.sinica.edu.tw/GuitarTranscription/>. No dataset license was
  found in the repository; public mirroring alone does not establish permission.
  Audio has not been redistributed into acceptance or declared licensed.
- `vincenzomadaghiele/vibrato-guitar`, commit
  `80bf2a3b07db64121a8bc1455aad595872c8ae5a`: actual labelled vibrato/static
  audio and motion-sensor data appear in its tree. No repository/dataset license
  was found. A paper's CC-BY icon/template does not establish the audio's terms.
  No audio has been added to acceptance on that assumption.
- `J-Atkinson/Sliding-Noise`, commit
  `bea3a53241ec99b5736d3b31409f5458f3f5858e`: two slide samples and several
  physical-model output WAVs. No license/README present; do not confuse model
  renderings with real fretted-slide recordings or infer reuse rights.

Earlier findings, including dummy mirdata GuitarSet WAVs, unpaired BasicPitch
annotations, NATSolo rendered outputs, and the EG-IPT README/LICENSE conflict,
are retained in `recorded_hammers/data-research.json`. They remain excluded.

## IDMT-SMT-Guitar follow-up

The [pinned source audit](source_research/idmt-2026-11-04/README.md) reads the
original authors' 2014 paper and three public repository references. The paper
establishes genuine manually annotated electric-guitar techniques, including
bending, slide and vibrato, but does not declare hammer-on/pull-off as dataset
classes. Its 50 ms onset / 200 ms offset evaluation tolerances are not physical
5/10 ms ground truth. The available GS-Detector mirror points to 234 isolated
normal-note WAVs; code/paper licenses do not establish the third-party audio's
terms. Original Fraunhofer/Zenodo metadata, archive hashes and license remain
unverified under the current network policy. No audio or new acceptance
coverage was adopted from that audit.
