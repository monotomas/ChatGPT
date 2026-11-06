# Independent trusted-reference bridge audit

The bridge recovers two expected G3 repicks, but introduces damping-related false events. It is not a musical pass.

The completed bridge capture records compile exit0 and run exit0,636 event rows across all48 configurations. NULLguard has576 rows across the same48 configurations. The driver, source inventory, patch, and both event CSVs are hashed in `independent-on-audit.json`. Only TimeDomainPursuit.cpp differs from NULLguard. This audit launched no compiler and edited no Source.

| Kind | Initial matches NULLguard→bridge | Second matches NULLguard→bridge | Extra Ons NULLguard→bridge |
|---|---:|---:|---:|
|0 held open6|72/72→72/72|—|0→12|
|1 replucked open6|72/72→72/72|10/72→12/72|6→10|
|2 replucked five-note chord|24/60→24/60|6/60→6/60|26→26|
|3 gain-ramp open6|72/72→72/72|—|0→12|

Across all408 authored On references, matched Ons rise256→258; extras rise32→60. The240 common initial matches and16 common second matches retain exactly the same output sample. No expected match is lost and no common expected On moves. The24 previously complete held/gain cases now all contain an extra On; no case is onset-complete after the bridge.

The only gained references are kind1,phase0,newGain1,dynamics0 and1,note55. Both emit atsample22399 (466.645833ms),45.645833ms after the authored G3 repick atsample20208 (421ms). Other kind1 second matches are unchanged: E2 six/12 (31.979–79.979ms delay),D3 four/12 (52.646–55.313ms); A2,B3,E4 remain zero/12. Kind2 second matches remain E3 six/12 (67.646–72.979ms); A2,A3,C4,E4 remain zero/12. Exact timings and misses for every note/configuration are in `independent-per-note-timing.csv`.

All12 held-control configurations and all12 gain-control configurations acquire a false E2 On atsample35071 (730.645833ms),50.645833ms after E2's authored damping at680ms. These false Ons occur after damping, not during the gain ramp. Kind1 additionally gains four late false E2 Ons forphase0 atsample50047 (1042.645833ms),after damping at1000ms. Its two existing phase.9/gain.5 late E2 extras move from50431 to50047 (8ms earlier); four existing wrong-note52 extras remain. Kind2 extras are unchanged. `independent-extra-ons.csv` retains every unmatched On for both variants.

The patch implements its stated mechanics: per-partial dt is clock−trustedSample; unreliable angles cannot replace trusted phase/omega/power. A valid reference survives only while NULL-unexplained and within128 analysis samples (10.667ms at12k). Return at exactly128 can compare; later return reinitializes. NULL-explained unreliable observations clear the reference. Existing held-set, nonmonotonic-clock, and excessive observation-gap resets remain. No threshold constant changed. Reliable NULL-explained returns can still learn trend and clear departure, as before; permanent coherence exclusions and longer gaps remain.

Matching independently reproduces the existing NULLguard totals: one fresh exact-note On is required per authored gesture; first-gesture windows end at the same-string repick, final windows end600ms after onset. A connected held note cannot satisfy the second gesture. Phase/newGain parameters are unused for kinds0/3, so their repeated configurations are not independent waveforms. These findings establish a localized recovery with control regressions, not broad polyphonic reliability or performance.
