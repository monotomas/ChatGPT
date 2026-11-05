# Native3 quiet-source alias baseline

This is a direct standalone detector diagnostic with **no new algorithm** and
no edits to main source or existing capture helpers. Eight CPP/eight matching
headers(including InputDynamics, excluding MpeOutputRouter) were copied from
current Native3 main with stable before/after hashes. The frozen copy stayed
unchanged through compilation and all24 captures.

Six unchanged sealed PCM files cover intended roots31/40/52 at common gains1/8.
Each is run with monoBend false/true ×inputDynamics enabled false/true at
48kHz/64samples, followed by250ms actual digital silence. All other detector
settings remain factory defaults: autoInput true, sensitivity0.65, release35ms,
**bend range2**. This does not silently reuse the legacy expression driver's
range12 or enable bend according to fixture labels. The expected tone starts
at24000 and continues until74400 for every file. PCM hashes, fixture manifest
hash and original seal are retained.

## Main finding: plain-mode failure already exists with dynamics off

Both quiet31 gains fail in plain mono with dynamics **disabled**, as well as
with it enabled. All four plain31 configurations emit wanted31, end it at40191
(837.31ms) and begin false43, while the wanted source lasts until1550ms. Thus
the original wanted note is712.69ms premature. This is not uniquely caused by
enabling the new dynamics stage.

The enabled/gain1 case reproduces the Native3 failure exactly:31On26367,
31Off/43On40191, additional43 Ons45951/51839. With dynamics off it starts31
at26239 and produces six43 Ons instead of three. Gain8 also produces six43
Ons with either dynamics setting. All unwanted On/Off times remain in the raw
trace and `summary.json`; first wrong On is40191 in every failing configuration.

With bend enabled, both dynamics settings retain31 through its actual source
cutoff, ending at76671(+47.31ms), with no extra On. All16 root40/52
configurations are clean single-note intervals:40 ends76415(+41.98ms),52 ends
76159(+36.65ms). No capture has overflow, orphan Off, duplicate note/voice On,
unclosed interval or active voice after supplied silence.

## All24 captures

Each cell is first usable wanted **On→Off samples; extra Ons**. Matching uses
the first correct interval with at least20ms overlap, not a later favorable
reacquisition. Raw pitch events and every status sample remain available.

| Root/gain | Plain, dynamics off | Plain, dynamics on | Bend, dynamics off | Bend, dynamics on |
|---|---|---|---|---|
|31/1|26239→40191;6|26367→40191;3|26239→76671;0|26367→76671;0|
|31/8|26239→40191;6|26239→40191;6|26239→76671;0|26239→76671;0|
|40/1|25471→76415;0|25471→76415;0|25471→76415;0|25471→76415;0|
|40/8|25471→76415;0|25471→76415;0|25471→76415;0|25471→76415;0|
|52/1|24959→76159;0|24959→76159;0|24959→76159;0|24959→76159;0|
|52/8|24959→76159;0|24959→76159;0|24959→76159;0|24959→76159;0|

All24 acquire the intended note at least once; four plain31 intervals end
prematurely and20 configurations preserve one wanted interval without extras.
Those facts must be reported together: acquisition counts alone conceal the
plain-mode octave alias and repeated wrong notes.

## Evidence and boundaries

`results/events.csv` retains every causal N/F/P event, native voice ID, velocity
and pitch. `results/status.csv` retains status and dynamics meter values after
every supplied block. `results/capture.csv` records each exact configuration,
source/processed length and final active state. `summary.json` retains all
intervals, every On, earliest wrong/extra On and per-configuration release
classification. No output is shifted or backdated.

`source-and-input-manifest.json` identifies all16 source files and six original
PCM hashes. `capture-result-manifest.json` binds the driver/executable and
successful compile/capture exits. The driver decodes only the same48kHz mono
PCM16 bytes; it performs no resampling or dynamics outside the detector.

No true-octave, articulation or unrelated noise counterexample was modified or
run here, and no proposed held-root fix is validated by this diagnostic. This
is factory-setting detector behavior; it does not independently execute the
full JUCE processor, host automation, calibration UI, Windows or Ableton.

To reproduce into fresh output directories:

```sh
g++ -std=c++17 -O2 -DNDEBUG -I Source capture.cpp Source/Pitch/*.cpp \
  -o /tmp/new-quiet-alias-capture
/tmp/new-quiet-alias-capture /workspace/ChatGPT/Tests/AudioFixtures/readiness \
  /tmp/new-quiet-alias-results
```

Only the eight retained standalone detector implementations are in this
snapshot. Shared CMake/Ninja state is not used. `manifest.json` hashes the
retained source/text evidence; the executable hash is recorded separately.
