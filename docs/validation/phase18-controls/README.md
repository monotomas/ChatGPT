# Qualified polyphonic repick filtering

The baseline is source17 at commit `b45d8a82203512512cd09bbb46caf9d31c24937e`.
This is a seen-data regression comparison plus additional articulation controls,
not another blind test. The original blind14 and blind16 reports remain intact.

The candidate retains all musical counts in the 196 development/disclosed cases.
Across 93 independent octave-damping controls, 36 held/inharmonic/withdrawal
controls and 24 genuine-repick/steady controls, no individual case gains a miss,
extra note, wrong note, premature release, hanging voice or release-P95 increase.
The raw before/after case and event tables are retained here. `summary.json`
records all aggregates, source hashes and the comparison criteria.

Two strictly past observations estimate the phase velocity of the fundamental
and odd partials. Fresh audio must then differ from that predicted phase before
an existing pending repick can restart its note. This avoids treating a biased
period estimate as a new attack, and avoids using the withdrawal of one upper
harmonic alone as evidence for a lower-string repick.

Fundamental-only comparison lost one genuine E2 repick. Accepting a change in
just the third harmonic falsely interpreted removal of a separate twelfth as a
new lower-string pluck. Both alternatives were rejected. The final condition
requires fundamental change or changes in at least two of odd partials 3/5/7,
after accounting for their pre-existing phase velocity.

Routine `FlysnapperPitch` now includes a held stiff E2, a lower E2 continuing
after upper-twelfth removal, and three genuine continuous E2 repicks, alongside
the earlier bright independent-release and recorded-octave tests. Their four
supplemental WAV files, annotations, source-generation recipe and SHA256 values
are in `Tests/AudioFixtures/regressions`. Regeneration reproduces all four files byte for byte. All 111,165 controls pass AddressSanitizer, UndefinedBehaviorSanitizer
and LeakSanitizer, with no sanitizer check disabled.

Three held-stiff-string restarts remain in the larger control set. Known
polyphonic misses, wrong notes and long acquisition delays also remain. These
measurements neither establish arbitrary-audio accuracy nor the user's physical
E2-under-10-ms/E3-under-5-ms objectives. Shared-VM CPU outliers are retained and
are not audio-driver or hard-realtime certification.
