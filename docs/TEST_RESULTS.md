# Flysnapper – dokumentasjon av verifikasjon

En Windows-installasjonspakke skal inneholde sine egne faktiske resultater under
`Validation/`. Det er disse filene som beskriver den konkrete bygde binæren;
historiske logger fra eldre kildecommits er ikke erstatning for en ny kjøring.

- `Validation/build-manifest.json`: produktversjon, plattform, kildecommit,
  låst JUCE-commit, faktisk MSVC/SDK-oppdagelse og Unicode-verktøyprøve, native
  Windows-status og SHA256 for hele VST3-bundlen.
- `Validation/test-results.xml` og `automated-tests.txt`: ferske CTest-resultater,
  med navn og resultat for hver obligatorisk test. Ingen manglende/skippede/feilede
  grupper kan telle som bestått.
- `Validation/audio-validation/summary.json`: de 30 faste grunnleggende
  lydkvalitetskontrollene, uten feilnoter, manglende noter, feil oktaver,
  hengende noter eller avvik fra de dokumenterte funksjonelle responsgrensene.
- `Validation/audio-validation/metrics.csv`, `events.csv` og
  `report-manifest.json`: observerte hendelser, første korrekte brukbare note,
  feilrate, slipp, CPU-frister og hashes for data/kode/evaluator som ble målt.
- `Validation/windows-audio-diagnostics/`: ferske native Windows-målinger av
  hele utviklingsmatrisen (729), første regresjonssamling (621) og andre
  regresjonssamling (414). Samme evaluator er bundet til den beståtte CTest-
  rapporten. Alle 1764 rader og hendelsesfiler må være komplette og konsistente;
  musikalske feil rapporteres og skal ikke omtales som bestått produktkvalitet.

Den faktiske Windows-installeren prøves også i en separat midlertidig mappe med
norske tegn: første installasjon, erstatning, verifisert sikkerhetskopi og lasting
av den **installerte** VST3-en i native testvert. Bevis/logg er publisert sammen
med den konkrete GitHub-leveransen. Dette er en faktisk fil-/modulprøve; ingen
systemmappe eller brukerinstallasjon er brukt i denne automatiseringen.

## Testgrupper

FlysnapperCore verifiserer alle relevante pitch-class-masker og MIDI-registre,
akkordgjenkjenningen mot referansen, alle 50×19 skalaoppsett og noteidentitet.
FlysnapperYin dekker estimatorens toneområde, harmoniske grunnlag, støy og de
åtte uavhengige akustiske gitaropptakene. FlysnapperPitch dekker den fullstendige
detektorens hendelser, mono/poly, samplerater, blokkvariasjon og reset.

FlysnapperPluginSmoke dekker tilstand, MIDI, GUI og grunnleggende sanntidsregler.
FlysnapperProcessorAudio sender faktiske sidechain-samples gjennom detektoren og
korreksjonen, samtidig med eksternt MIDI. FlysnapperActualVst3 laster den bygde
VST3-modulen og kontrollerer SDK-busser, sampleposisjoner, audio/MIDI, state,
transport og kildeavslutning. FlysnapperAudioQuality er den separate, frosne
musikalske kontrollen; at en målerunner bare avslutter med kode0 betyr ikke at
lydkvaliteten er godkjent.

Python- og PowerShell-tester dekker verktøyoppdagelse, Unicode, filinventar,
signatur-/hashvern, transaksjonell installasjon, gjenoppretting, andre plugins,
feiltilfeller og bindingen mellom faktisk Windows-bygg og GitHub-leveransen.
Fixturetester med en falsk PE-header hevdes aldri å ha lastet en Windows-plugin.

## Målegrense

Den frosne grunnkontrollens funksjonelle maksimum er ikke brukerens 5/10ms-mål.
Fysisk live-forsinkelse inkluderer driver/konvertering, pitchobservasjon, vertens
ruting, mottakerinstrument og lydutgang. CPU-tid eller rapporterte latency-samples
kan ikke erstatte denne målingen. Se AUDIO_VALIDATION.md for lyddataenes opphav,
fullstendige statistikker og begrensninger.

Ingen faktisk Ableton- eller lydkorttest erklæres utført uten et slikt resultat.
Gamle resultater fra 0.1.0 er bevart under history/ og validation/ som historikk,
inkludert en tidligere rutingsprototype. De dokumenterer ikke den nye detektoren.
