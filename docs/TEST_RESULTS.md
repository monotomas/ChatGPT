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
Fra 0.2.1 kreves i tillegg en erstatning startet gjennom den faktisk pakkede
`INSTALL_FLYSNAPPER.bat`, med mellomrom, norske tegn, parenteser og ampersand
i stien. Prosessens exitkode, installasjonsmelding, nye sikkerhetskopi og filhashes
kontrolleres før den installerte modulen lastes. En direkte PowerShell-prøve
alene oppfyller ikke dette kravet.

## Testgrupper

FlysnapperCore verifiserer alle relevante pitch-class-masker og MIDI-registre,
akkordgjenkjenningen mot referansen, alle 50×19 skalaoppsett og noteidentitet.
FlysnapperYin dekker estimatorens toneområde, harmoniske grunnlag, støy og de
åtte uavhengige akustiske gitaropptakene. FlysnapperPitch dekker den fullstendige
detektorens hendelser, mono/poly, samplerater, blokkvariasjon og reset.

FlysnapperGuitarQuality bruker de 55 forseglede kontrolltilfellene under
`Tests/AudioFixtures/guitar_controls`: 51 tilfeller med faste noter ved to
blokkstørrelser, samt fire kontinuerlige bend-/vibratokurver ved to bendområder
og to blokkstørrelser. Den sjekker toneforløp, ekte demping og oktavskifter,
falske noter, pitchfeil mot den uavhengige kurven og samme beslutninger uansett
blokkdeling. De er kjente regresjonsprøver etter at feilene i 0.2.0 ble målt;
de utgjør ikke en ny blindtest av vilkårlig gitarspill.

FlysnapperBendProcessor sender audio gjennom den faktiske prosessoren og måler
MIDI pitch wheel, RPN, ankernoter, reanslag, uttrykk på kanal 1 og 2, tilstand og
avslutning ved modusendring, bypass og Panic. Den inkluderer de fire uavhengige
harmoniske kurvene og en kjent avledning med 48 dB avklinging samtidig med bend.

FlysnapperPluginSmoke dekker tilstand, MIDI, GUI og grunnleggende sanntidsregler.
FlysnapperProcessorAudio sender faktiske sidechain-samples gjennom detektoren og
korreksjonen, samtidig med eksternt MIDI. FlysnapperActualVst3 laster den bygde
VST3-modulen og kontrollerer SDK-busser, sampleposisjoner, audio/MIDI, state,
transport og kildeavslutning. Fra 0.2.1 rekonstrueres dessuten tonehøyden fra
den kompilerte modulens faktiske MIDI-wheel ved tre samplerater og to bendområder,
samtidig som eksterne noter og wheel-meldinger på kanal 1 kontrolleres.
FlysnapperAudioQuality er den separate, frosne
musikalske kontrollen; at en målerunner bare avslutter med kode0 betyr ikke at
lydkvaliteten er godkjent.

Alle ni grupper er obligatoriske for 0.2.1. Resultater fra de opprinnelige sju
gruppene alene kan ikke brukes som verifikasjon av denne oppdateringen.

Python- og PowerShell-tester dekker verktøyoppdagelse, Unicode, filinventar,
signatur-/hashvern, transaksjonell installasjon, gjenoppretting, andre plugins,
feiltilfeller og bindingen mellom faktisk Windows-bygg og GitHub-leveransen.
Fixturetester med en falsk PE-header hevdes aldri å ha lastet en Windows-plugin.

## Nativ kontroll av 0.2.1

Den endelige Linux-kilden ble kontrollert uendret gjennom bygg og alle ni
CTest-grupper: 9/9 bestått på 35.53 sekunder. Bevis og hashes for 30 kilde-/
testfiler ligger under `validation/integrated21`. Den faktisk lastede VST3-en
bestod 952458 kontroller, inkludert bend/vibrato ved tre samplerater og to
bendområder. Den separate prosessorprøven bestod 477610 kontroller.

De samme seks DSP-filene bestod AddressSanitizer, UndefinedBehaviorSanitizer
og LeakSanitizer med 111165 pitchkontroller og 93927 gitarkontroller; ingen av
disse instrumenteringene var slått av. Se `validation/sanitizers21` for nøyaktig
scope, compilerflagg og hashes. Dette gjelder den selvstendige detektoren,
ikke hele JUCE-/driver-runtime.

Den endelige native VST3-en bestod også pluginval nivå 10: 25 grupper, GUI og
parameterfuzz på, 13.27 sekunder. Pluginval rapporterte sin valgfrie eksterne
Steinberg-validator som skippet fordi et slikt verktøy ikke var oppgitt.
152 Python-tester ble kjørt: 151 bestått, én Windows-spesifikk BAT-prøve skippet
på Linux. Den prøven er obligatorisk i Windows-jobben. Native Windows-resultater
må fremdeles leses fra den konkrete pakkens egne `Validation`-filer.

## Målegrense

Den frosne grunnkontrollens funksjonelle maksimum er ikke brukerens 5/10ms-mål.
Fysisk live-forsinkelse inkluderer driver/konvertering, pitchobservasjon, vertens
ruting, mottakerinstrument og lydutgang. CPU-tid eller rapporterte latency-samples
kan ikke erstatte denne målingen. Se AUDIO_VALIDATION.md for lyddataenes opphav,
fullstendige statistikker og begrensninger.

Brukeren har bekreftet at sidechain, MIDI-utgang og all ruting fungerer i
Ableton med 0.2.0. Det bekrefter vertsrutingen; det bekrefter ikke den nye
detektorens musikalske kvalitet eller fysisk forsinkelse. Ingen automatisert
Ableton- eller lydkorttest er utført i skymiljøet.
Gamle resultater fra 0.1.0 er bevart under history/ og validation/ som historikk,
inkludert en tidligere rutingsprototype. De dokumenterer ikke den nye detektoren.
