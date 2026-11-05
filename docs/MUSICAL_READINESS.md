# Flysnapper 0.3.0 – gjeldende evalueringsstatus

Brukerens bestilling 5. november gjelder en kontrollert versjon for prøve med
egen gitar. Polyfonisk bending og absolutte latency-grenser er trukket tilbake;
monofonisk bending, statiske polytoner, støy/utklang, kalibrering og hjelpetekster
beholdes. Se [gjeldende oppdrag](PRODUCT_SPEC.md) og
[0.3.0-notatet](RELEASE_0.3.0.md).

Det ferske [Native4-kontrollpunktet](validation/dynamics-0.3.0/native-attempt-4/README.md)
består **15/15 grupper**, inkludert **86 processorprøver** med kausale
MIDI-tidspunkter, faktiske vertsparametre, svake toner ved brum, slipp, bends,
ekte oktavskift og kalibrering. Kilde- og filhashes er bundet til byggingen.
Dette kvalifiserer kildekandidaten for Windows-bygg og den bestilte gitarprøven,
uten å hevde generell musikalsk feilfrihet eller allerede utført Live-prøve.

[De brede diagnostiske prøvene](validation/dynamics-0.3.0/README.md) beholder
alle feil: **98/209** består, og polyfonisk støy-/slippadferd har fortsatt
begrensninger. De er separate fra de obligatoriske, avgrensede programtestene.
Windows-installasjon krever sine egne native bevis i leveransens Validation-mappe.
Den historiske forskningssperren nedenfor gjelder 4. november-oppdraget og
dets kildepunkt, ikke den reviderte 5. november-evalueringen. Ingen gammel
målepolicy eller feil er endret for å lage et nytt beståttresultat.

---

# Musical readiness work after 0.2.1

> **5 November scope update:** The user has removed polyphonic bending and
> absolute latency limits, and now requests a validated version for a real
> guitar trial. The current work is 0.3.0: quantised poly notes, mono bending,
> input dynamics with guided calibration/manual controls, and contextual help.
> See the current [product scope](PRODUCT_SPEC.md). The results and release
> decisions below describe the **4 November research checkpoint**. They remain
> intact and do not constitute an evaluation of the new dynamics frontend.

This work is on `research/musical-readiness`. It is unfinished research, not a
new installer or a claim of musical acceptance. The previous 0.2.1 installer
does not meet the user's November 4 requirements. No new release version has
been assigned.

## Acceptance evidence

The [policy](../Tests/AudioFixtures/readiness/policy.json) was defined before
measuring the new implementation. The independently prepared controlled
edition contains 38 PCM recordings and 72 mono/poly cases. Its seal is
`d5af9fb3fefb4d41918e62810d59290c1dd741d95337d58ec34efcdcf14f9fba`.
Keep that edition, the previous 30-case packaging gate, and previous disclosed
corpora intact. Once examined during tuning, a corpus is a regression set;
it is no longer an unseen final test.

Scoring must retain actual decision timestamps, one-to-one note/voice
matching, misses, extra notes, wrong notes before correct notes, release
errors, pitch-curve error and execution deadlines. Do not backdate events or
align a delayed bend curve to truth before scoring it. Implementation voice
IDs need not equal physical string IDs. Synthetic success alone does not
establish musical readiness.

The policy requires independent real-guitar evidence for fretted slides,
hammer-ons, pull-offs, bends, vibrato and handling/squeak noise. Existing
isolated guitar recordings, constructed mixtures and artificial interference
are valuable controls, but do not supply all those recorded techniques.
The original E2 <10 ms/E3 <5 ms complete-chain goals remain unverified.

## Work in progress

* Causal stationary mains cancellation and gain-relative tonal/noise evidence.
  Mechanism tests include cold/ramped hum, quiet protected notes near mains
  frequencies, simultaneous 50/60 Hz families and gain changes. Passing these
  tests is not a general classifier of a musician's intentions.
* Causal mono acquisition and per-voice articulation evidence. Fretted steps
  and weak new excitations must be distinguished from fast smooth bends,
  without declaring every pitch change a new note.
* Continuous polyphonic source tracking. Harmonic overlap, false subharmonics,
  new-source admission and independent pitch curves remain substantial
  unresolved work. A successful two-tone example is not sufficient.
* Optional standard MPE output. Generated voices and ordinary external
  playing notes occupy separate member channels. Core scale correction,
  note bindings, pedals, stealing, reset and mode changes must remain correct.
  JUCE MPE receiver tests check the stream; they are not an Ableton test.

No aggregate readiness result has passed. Do not turn mechanism check counts,
an existing release's validation, or partial latency gains into a new release
claim. Full native/Windows/installer verification follows a frozen candidate
that meets the audio requirements; it cannot substitute for those requirements.
The builder currently marks every collected result as a research prototype;
the Windows packager rejects that stage. This explicit release hold remains
even if all protocol/unit tests pass. Retained failed model experiments and
host reviews are in [the research evidence](validation/musical-readiness-research/README.md).

## Frozen current-source results

The [exact current-source acoustic checkpoint](validation/main-readiness-checkpoint/README.md)
uses the Pitch source at `ddceefbf9a49f2f265cb692863ab43ad4c2389a9`,
mono-v20/poly17. Its three captures completed successfully and retained their
failing score exits, raw note/pitch events and source hashes. These are seen
regression sets at 48 kHz/64 samples, before MIDI correction and routing.

| Suite | Mono configurations passed | Poly configurations passed | Total passed |
| --- | ---: | ---: | ---: |
| Controlled signals | 29/34 | 2/38 | 31/72 |
| Real isolated hammer samples | 2/8 | 3/8 | 5/16 |
| Recorded noise/quiet-note controls | 34/64 | 3/64 | 37/128 |
| All configurations | 65/106 | 8/110 | 73/216 |

All 16 quiet real mono plucks pass. This does not establish robust behavior:
the 32 real mono note/noise mixtures still produce 48 extra onsets and 28
premature first releases. Poly noise-only controls generate 65 onsets. The
controlled mono slide and hammer/pull families retain all intended notes,
but the fastest bend and music/handling cases still fail. Broader human
technique coverage remains missing. Conditional matched-note latency is
reported with these failures; it does not count a missed note as a fast one.

The separately retained
[native mono-v20/poly17 checkpoint](validation/musical-readiness-research/native-checkpoint-v20/README.md)
passed 12 of 13 groups with matching compiled-source hashes. Strict
simultaneous poly source acquisition failed. Protocol and functional passes
cannot approve the failed acoustic results. There has been no new frozen
Windows, installer or Ableton acceptance run for this research candidate.

## Rejected alternatives

These bounded investigations preserve source snapshots, raw failures and
reproduction instructions. None supplies a replacement ready for adoption:

* [Monophonic continuity](validation/mono-continuity-research/README.md): one
  variant reduces recorded-noise extras from 50 to 24 but loses three genuine
  repicks. Another restores all 107 older reference notes while retaining 40
  extras and 28 early releases. Both falsely rearticulate a fast low bend.
* [Harmonic continuity observer](validation/harmonic-continuity-research/README.md):
  a bounded, allocation-free mathematical mechanism passes ideal-frequency
  checks; actual detector frequency/history uncertainty defeats the intended
  noise distinction. Its unit success is not end-to-end musical success.
* [Dynamic poly models](validation/poly-research/dynamic-models-v2/README.md):
  joint frequency/chirp estimation improves some bends, but the best strict
  score is 4/8 and costs about 6.6 ms P95 on the shared worker. Weak tails,
  phase resets and execution deadlines remain unresolved. Correct initial
  voices were deliberately supplied; these are not acquisition results.
* [Additional-source admission](validation/poly-admission-research/README.md):
  a physical model can recover a clean overlapping second source, but timbre
  changes still invent an extra octave. The offline optimizer is too slow for
  an audio callback and noisy early acquisitions still fail.
* [Poly mains-only admission veto](validation/poly-mains-admission-veto/README.md):
  reusing positive mains evidence removes four pure-hum onsets, but also loses
  three previously found wanted notes in handling/quiet-addition controls.
  Aggregate case passes increase while those genuine-note regressions remain.
  The candidate is rejected; the unchanged current source is still measured
  in the table above.
* [Audited streaming neural alternative](research/causal-alternatives/README.md):
  the published integer-note model has no independent continuous bends and
  produces many false notes on the same recorded inputs. A bounded post-hoc
  threshold study cannot retain the existing poly recall/extra-note baseline;
  fewer errors come with missed notes or substantial delay. No external
  runtime or model weights have been incorporated into the product.

All data used in those comparisons is now seen development data. No failed
variant has been silently substituted into the current-source checkpoint,
and none justifies removing the release hold.

## Host evidence

The pinned [Live 12 manual copy](https://github.com/J0rgeSerran0/Ableton_Live_Manual_to_HTML_and_PDF/blob/4bb26b7814a581cf6d460ff4e5f04036daef23fe/docs/Ableton_Live_12-en.md)
states in section 10.5 that MIDI-output plug-ins with MPE enabled can output
MPE. It documents feasibility, not execution of this build in Live. Incoming
playing/chord channels after enabling source-device MPE, and preservation of
per-note pitch through the receiving device route, are not yet runtime
verified. Existing ordinary sidechain and MIDI routing was confirmed by the
user before this work and should be preserved.

## External recordings

The pinned [Guitar-TECHS site](https://github.com/guitar-techs/guitar-techs.github.io/tree/19a2954f789bfd192b2b4732788ceb000c1dd687)
describes the [Zenodo dataset](https://zenodo.org/records/14963133), a potential
source of labelled recorded techniques. The current cloud network policy
blocks `zenodo.org` and the original `www.idmt.fraunhofer.de` source for the
IDMT guitar technique recordings. Additive domain changes have been saved in
the cloud configuration draft; that draft does not change the running policy.
These access limits are separate from the demonstrated algorithm failures;
access to more recordings would not itself fix them. Continue
with legitimately available sources without bypassing the policy. Verify
the actual source repository, license, file hashes and annotations before
using or redistributing any recording.
