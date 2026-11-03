# Flysnapper 0.1.0 – testresultat, 3. november 2026

**Status:** Kompilert og automatisk testet rutings-/MIDI-prototype på Linux.
Dette er ikke en ferdig Windows-/Ableton-godkjenning og ikke en pitchdetektor.

## Faktisk utført

Miljø: Debian 13, Linux x86_64, GCC 14.2.0, Python 3.12.14, CMake 4.4.4,
Ninja 1.13.2, JUCE 8.0.16-security på eksakt commit
`ea77444750de45e036e0279bb0291144bf8e3688`. Virtuell X11-skjerm brukes for GUI.
Opprinnelig handover og referansens SHA256-kontrollsummer ble verifisert.
Buffkiller-referansearkivet er uendret og inngår som referanse i kildepakken.

| Kontroll | Resultat | Hva det faktisk demonstrerer |
|---|---|---|
| FlysnapperCore | 6 grupper, 2 104 154 assertions, bestått | Alle ikke-tomme pitch-class-masker mot alle MIDI-noter, grenseverdier, seks tie-eksempler, numerisk akkordgjenkjenning mot hele referansen, alle 50×19 regler og 12 grunntoner, livssyklus, sustain/kollisjoner og overflow |
| FlysnapperPluginSmoke | 225 603 assertions, bestått | Faktisk prosessor og editor; 50×19 state-roundtrip, stabile ID-er, isolerte instanser, blokkinterne akkorder, bypass/transport, variable blokker, null-frame reset, avgrenset overflow og flip uten noteavbrudd |
| FlysnapperActualVst3 | 28 010 assertions, bestått | Den kompilerte .vst3 lastes i en egen JUCE-testvert; instrumentklassifisering, SDK kAux-sidechain, event inn/ut, kanalroller, notevelocity og sampleoffset, state, transport og samtidige genererte/eksterne noter |
| Python-bygger | 72 tester, bestått | Fixturebaserte kontroller av verktøyoppdagelse, Unicode-tilpasning, tester-før-installasjon, hashkontroll, gjenoppretting og kildebeskyttelse; også kjørt inne i byggerens faktiske lokale TEMP-oppsett |
| Kildepakking | 9 tester, bestått | Påkrevde filer, include/exclude, sikre stier, deterministisk ZIP og SHA256-integritet |
| Kjerne ASan/UBSan/LeakSanitizer | Bestått | Avhengighetsfri C++-motor med sanitizer-instrumentering; gjelder ikke hele JUCE-/VST3-wrapperen |
| Skyoppsett | Bestått | Isolert APT-/sysroot-oppsett uten systemendringer, signatur-/SHA256-kontroll, låste byggeverktøy og aktiveringsskript |

VST3-hendelser er prøvd ved 44,1/48/96 kHz og 32/128/257 samples. Intern
100 ms-testnote er i tillegg prøvd i prosessortesten ved tre samplerater og fem
blokkstørrelser. Den faktiske VST3-wrapperen bevarte sampleposisjon 17 ved
prøvde CC1, pitch bend, kanalpressure og program change. Dette er lokale
adaptertester, ikke et løfte om alle vertsprogrammers uttrykksstøtte.

`validation/native-ctest.txt`, `validation/native-ctest.xml` og
`validation/native-manifest.json` er resultatene fra den aktuelle kjøringen.
`screenshots/` inneholder faktisk rendret editor fra Linux-testen; ingen av
bildene er fra Ableton. `UI_SKETCH.svg` er bare en illustrasjon.

## Ren utpakking og gjenbruk

Hele kildepakken ble pakket ut i `/workspace/clean-validation/Olav Lægreid/Flysnapper-0.1.0`.
Alle byggemål ble bygget fra ny build-katalog med samme verifiserte JUCE-kilde.
Etter den avsluttende pedalrettelsen ble hele den oppdaterte kildepakken pakket
ut på nytt og alle berørte mål og testgrupper kjørt igjen. Alle tre CTest-grupper
består også der; se `validation/clean-unicode-ctest.txt`. Kompilerbar kilde,
byggeskript og avhengighetslås er kontrollert byte for byte mot leveransen.
Dette er en Linux Unicode-stiprøve, ikke Windows resource-/MSVC-verifikasjon.

`Tools/cloud_validate.sh` ble også prøvd uten eksisterende skjermprosess. Det
startet Xvfb, verifiserte klarhet, bygget/testet og avsluttet sin egen prosess.
`install_script` og `start_skill` er lagret i miljøutkastet. Dette er ikke
publisering eller bevis for gjenoppretting i en ny skyoppgave.

## Avvik som ble undersøkt og rettet

Sluttgjennomgangen fant at videresendt CC66/69 kunne holde lyd i mottakeren
etter at interne note-bindinger var sluppet. Global reset husker nå slike
pedaler og sender pedal opp, også uten aktive innnoter. En egen motor- og
faktisk VST3-regresjon kontrollerer oppryddingen. Normal videresending og
kildespesifikk notebinding endres ikke.

Den første VST3-stateprøven sendte en normalisert verdi mellom to lovlige
menyvalg. JUCEs vertsparameter mellomlagret den sendte verdien, mens prosessoren
korrekt lagret nærmeste diskrete valg. Testen velger nå lovlige trinn og krever
både at kilden tok imot det tilsiktede valget, og at en ny instans gjenoppretter
nøyaktig samme valg. Ingen state-assertion ble fjernet eller toleranse svekket.

Byggeskriptets egne installasjonstester måtte også isolere sin fiktive
kildemappe fra sin fiktive pluginmappe når TEMP ligger under prosjektet. Den
virkelige sperren mot installasjon i source/build/dist er beholdt og har fått en
egen regresjonstest. Sluttkjøringen passerer alle obligatoriske CTest-grupper.

## Tids- og sanntidsgrenser

Ingen ekstra blokk eller lookahead er lagt inn for MIDI-korreksjonen.
Rapporterte 0 latency-samples er **ikke** bevis for null fysisk forsinkelse.
Ingen audio-driver, gitar, mottakersynth eller akustisk kjede er målt her.

`CORE_BENCHMARK.md` oppgir median, P95/P99, maksimum og blokkfristbrudd ved
48 kHz og 32/64/128 samples. Den vanlige fire-hendelseslasten hadde median
0,681 µs i denne delte VM-en. Den kunstige 2048-hendelseslasten hadde observerte
fristbrudd; den skal ikke brukes som sanntidsgaranti. Dette er CPU-tid for
MIDI-kjernen og har ingen pitch-to-MIDI-latencypåstand.

Kjernen hadde null C++ new/new[]-allokeringer gjennom 10 000 testiterasjoner.
Oppvarmet prosessor ble også kontrollert for C++ new. Disse kontrollene fanger
ikke malloc/realloc i JUCE/vertsadapteren. Privat output-buffer forhåndsallokeres;
vertsprogrammets leveringsbuffer kan vokse. Hele VST3-kjeden er ikke erklært
allokeringsfri.

## Gjenstår før produktet kan godkjennes

- Faktisk Windows x64-/MSVC-bygg, Windows Unicode-resourceprobe og installasjon.
  Byggeren og en manuell Windows GitHub Actions-arbeidsflyt følger med; de er
  ikke kjørt som Windows her. Ingen Windows-binær leveres i denne pakken.
- Ableton Live 12 Suite: direkte kanal-16-ruting, sidechain/No Input, MIDI-utgang,
  lagre/gjenåpne, MPE-konflikt, deaktivering og fjerning. Følg HOST_TEST_PLAN.md.
- Monofon og polyfon audio-til-MIDI, anslag/slipp, velocity og reelle gitarprøver.
  Dette er bevisst utsatt i tråd med handoverens rutingport.
- Fysisk end-to-end-latency og brukerens 5/10 ms-mål. Ikke målt.
- Godkjenning av GUI og de åpne musikalske reglene i PROTOTYPE_POLICIES.md.

Kildepakken er laget for videre bygging og test uten at brukeren må redigere
kildekode. Linux-valideringsbinæren holdes separat og kan ikke installeres i
Windows Live. Ingen plugin er installert og ingen kilde er pushet til GitHub.
