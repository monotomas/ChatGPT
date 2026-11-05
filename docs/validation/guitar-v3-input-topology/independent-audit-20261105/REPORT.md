# Independent post-NC topology audit

No main source, original topology snapshots or existing captures were changed.
All new outputs are in this `independent-audit-20261105` directory. The audit
has two parts: independent interval/release analysis of three existing frozen
topologies, and a newly compiled dynamics-disabled detuning baseline using the
exact frozen raw-reference implementation. No new DSP variant was invented.

## Input integrity and method

All17 bound source/driver files in each topology's `source-manifest.json`
verify(51 checks total). The existing capture/scorer inputs and frozen source
hashes are recorded in `input-bindings.json`. Important detector CPP prefixes:

| Frozen topology | PitchDetector.cpp SHA-256 prefix |
|---|---|
|host-before-nc|8139b4025ef16326|
|analysis-after-nc-raw-reference|5e5b301cb289c21e|
|analysis-after-nc-shared-protection|333d1c58ea329340|

All three use the same frozen InputDynamics.cpp hash
`f2605f9aaf9395eda6507d79f7bb0de785b2cfdd450e18e0ab90b9c1a74294c1`.
Full source and driver hashes remain visible in the manifests; directory names
alone are not source identities. Mutable project InputDynamics files were not
used for compilation or measurement.

The duration audit independently pairs `N`/`F` events by **(voice_id,note)** and
ignores pitch-only `P` events. It retains every noise-only interval, sums voice
duration(including overlapping wrong voices), and separately calculates the
union duration(time with at least one wrong voice). Actual supplied flush
frames are included; no source EOF is substituted for a recorded Off. All six
variant/suite combinations have zero orphan Offs, duplicate same-note/voice
Ons and unclosed intervals. Captures supply250ms silence after each source.

An independent implementation also reconstructs one-to-one usable-reference
matching, per-reference decision/Off times, early and late releases from the
original fixture annotations and unchanged November5 policy. All reconstructed
counts and matched reference decisions agree with the stored scorer: **zero
reconstruction mismatches**. All early/late/miss changes, including offsetting
recoveries/regressions, are retained in `topology-duration-audit.json` under
pairwise case/reference changes. No latency alignment or backdating is used.

Controlled scope is65 of72 configurations; the seven historical poly-bend cases
remain explicitly out of scope under the user's revised request. Recorded-noise
scope is all128 configurations. These are seen regression sets. Noise duration
sums configurations, including mono/poly views of the same recording; it is
not the length of independent human performances.

## Noise-only durations: fewer Ons and shorter exposure are distinct

| Controlled65 observation | Host before NC | Raw reference after NC | Shared protection after NC |
|---|---:|---:|---:|
|Noise-only Ons|24|32|32|
|False voice duration|7.520s|5.912s|5.912s|
|False union duration|6.693s|4.931s|4.931s|
|Matched references /151|130|129|133|
|All extra Ons|84|74|74|
|Early Offs|9|8|8|
|Late Offs|4|2|6|
|Cases passed /65|35|42|42|

The controlled noise-only differences occur in poly mode; all three mono
paths emit no noise-only notes on these controls. The two after-NC variants'
noise-only duration and event-count totals are identical. Their extra matched
notes concern musical intervals, not improved noise-only suppression.

| Recorded-noise128 observation | Host before NC | Raw reference after NC | Shared protection after NC |
|---|---:|---:|---:|
|Noise-only Ons|43|42|42|
|False voice duration|6.827s|6.699s|6.699s|
|False union duration|4.984s|4.997s|4.997s|
|Matched references /96|96|96|96|
|All extra Ons|245|240|240|
|Early Offs|45|43|43|
|Late Offs|3|3|3|
|Cases passed /128|47|48|48|

Recorded-noise voice duration falls slightly, while union exposure **increases**
about13.33ms. This must not be described as uniform noise-duration improvement.
Mono/poly subtotals, every interval and every noise-case duration are retained.

## The matched-note gain is accompanied by bad releases

Raw-reference129→shared133 recovers four previously missed references. All
four recovered references release late:

| Controlled poly reference recovered | Release after truth |
|---|---:|
|music_handling_snr12_gain1, A3/57|637.98ms|
|music_handling_snr24_gain1, A3/57|88.65ms|
|music_handling_snr24_gain4, A3/57|88.65ms|
|quiet_new_note_after_decay_noise, second A3/57|95.98ms|

Against the host-before-NC path, the net130→133 match gain consists of:

* quiet_hum_31_gain8_poly: newly matched, but ends648.69ms **before** truth;
* music_handling_snr12_gain1_poly: newly matched, but637.98ms late;
* quiet_new_note_after_decay_noise_poly: newly matched second note,95.98ms late.

The host path already matched both24dB music cases and released them at
+72.65/+67.31ms. Shared protection makes both+88.65ms, crossing the unchanged
80ms release criterion. Therefore improved aggregate recall is not proof that
these newly matched cases are musically solved.

There are useful changes too: host→both after-NC variants fixes premature mono
releases in quiet_hum_40_gain1 and quiet_hum_52_gain1; quiet_hum_31_gain1_mono
release improves145.98→47.31ms. Two recorded mixtures lose their premature
release: acoustic A3+muted strings12dB gain160 mono, and electric E2+percussion4
12dB gain160 poly. The complete JSON retains these improvements together with
every regression, including timing changes that do not cross a pass threshold.

## Dynamics-off detuning baseline: six new high-edge misses confirmed

`detuning-enabled-driver.cpp` is a byte-exact copy of the parent's876-case
probe. `detuning-disabled-driver.cpp` differs by **one boolean only**:
`s.inputDynamics.enabled=true` becomes`false`. Both refer to the same frozen
raw-reference Source; no mutable main files are compiled. The disabled probe
was newly compiled/run, returns0 and verifies all16 CPP/H hashes before/after.
The enabled876-row CSV is retained original evidence, not represented as a new
enabled run. Its driver/source/executable hashes are explicitly bound in
`detuning-baseline-plan.json` and `detuning-baseline-result.json`.

The matrix is all73 declared notes28..100 ×±40cents ×pure/bright ×44.1/48/96kHz,
64-sample callbacks. Both CSVs have exactly876 unique matching configurations.

| Detuning observation | Dynamics disabled | Dynamics enabled |
|---|---:|---:|
|Strict passes(one correct On/Off, no wrong On/hang)|861|855|
|Misses|15|21|
|Wrong note-ons|0|0|
|Active voices after supplied silence|0|0|

All15 dynamics-off misses are shared: note31+40cents pure/bright,43+40cents
pure/bright and47−40cents pure, at all three rates. They are already present
when dynamics is bypassed in this frozen implementation, so cannot all be
attributed to enabling the frontend.

The **six new misses** are note100+40cents, pure and bright, at all three
sample rates. Dynamics-off produces the correct On at samples470/511/1023
(44.1/48/96kHz); enabled emits no note. There are no recovered misses. These
new high-edge failures must be fixed or explicitly accounted for before a
claim of preserved declared-range behavior.

There are also45 changed first-On times among the855 configurations acquired
by both paths: all45 are slower with enabled dynamics,810 are identical and
none are faster. Added delay spans2.67–53.33ms. The largest changes are notes
28–30(+42.7–53.3ms); the others are33 and92–100(+2.7–10.7ms). Notes40–88 are
unchanged in this particular synthetic probe. The six new misses are retained
as misses and excluded only from this explicitly conditional timing comparison.
No claim about physical/audio-interface latency follows from these timestamps.

## Audit disposition and reproducibility

This report does not choose or integrate a topology. The after-NC paths have
some useful acoustic improvements, but shared protection's recall gains carry
late tails and enabled raw-reference introduces six new high-edge misses.
Those concrete failures remain visible. The root agent must assess them before
adopting a complete-product change; score counts alone are insufficient.

`topology_duration_audit.py` recomputes durations and independently reconstructs
reference decisions from the frozen raw event files and repository fixture/
policy paths. Its input file hashes must agree before repetition; copy the
audit to scratch for new outputs. `detuning-comparison.json` retains all876-case
classifications, all15 shared misses, all six new misses and every changed row.
The raw disabled CSV and stdout, compile command, source bindings and exact
drivers are retained. No original raw/scored output was overwritten.

`manifest.json` binds all new retained text records and records the new compiled
executable hash separately. Original source/capture bindings live in
`input-bindings.json`; source snapshots remain in their original topology
directories. Binaries, PCM and dependency/model payloads are not copied into
the retained text inventory. These cloud-native results are not Windows, Live,
hardware or universal musical-readiness approval.
