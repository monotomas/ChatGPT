# Endelige brukerbestemte fabrikkvalg

Dette kontrollpunktet gjelder de 52 valgene fra brukerens opplastede
[Excel-fil](../../factory-settings/2026-11-07-user-values/README.md).
Alle ID-er er koblet uavhengig av radrekkefølge. 20 fabrikkvalg er endret,
32 beholdt og fire tillatte valg uten alle akkordtonene er bevart nøyaktig.
Skalatyper, intervaller, akkordgjenkjenning og MIDI-korreksjonsregler er uendret.

Opplastingen er 18 622 byte, SHA-256
`2df3afd9c7757469d8d7bf524efc3e2e09eb03824452ecbeb97294e3fafcc231`.
Den kanoniske mappingens SHA-256 er
`146501dd5146f620c6718bc2be9b784380cc22eef91ec1e9dac1251c5422e814`.
Full import-JSON med metadata har egen hash, registrert separat i
`factory-check.json`; den må ikke forveksles med mappingens hash.

- [Core](core/result.json): 6 grupper og 2 191 256 kontroller består;
  10 000 realtime-iterasjoner uten nye allokeringer. Åtte utvalgte inndata
  er registrert før kompilering og etter kjøring med like hashes.
- [Samlet native-kjøring](native/full-validation.txt): 4/4 CTest-grupper
  består. GUI har 20 293 kontroller. Prosessoren har 109 494 / 14 grupper /
  2 379 rå hendelser. Den faktiske VST3-modulen har 5 373 rapporterte
  kontroller / 22 tilfeller / 1 120 rå hendelser; stdout har én ekstra
  kontroll av den siste rapportskrivingen.
- [Python](python/result.json): 198 totalt, 196 bestått og to forventede
  native Windows-BAT-hopp på Linux. Ingen feil.
- [Skjermbilder](screenshots/) viser brukerens faktiske fabrikkvalg og er
  visuelt kontrollert. Lange Harmonic Major-navn får plass. De tilsiktede
  kompatibilitetsmarkeringene beholdes.

Linux-modul SHA-256:
`6ebe6efa9e7147ddbb8d2548312a2c6ce65947222424f3db0b306d8dc0c599a2`.
Den kompilerte modulens deklarerte 52 standardvalg kontrolleres mot en
uavhengig, bokstavelig testoracle avledet fra brukerfilen. Tilstand lagres
før prosessering og uten tilordningssettere, og gjenopprettes i en bevisst
endret kopi. Reelle fabrikkvalg ved indeks 32 og 35 brukes i MIDI-prøver.
Rapporten binder `user_factory_defaults` og mappingens hash etter kontrollene.

Den tidligere indeks-3-regresjonen er flyttet til brukerens faktiske
CmMaj9→Harmonic Minor-valg. C5→Minor Pentatonic ved indeks 12 prøves også.
Begge bruker fersk konstruktør, faktiske MIDI-resultater og eksporterte
skala-ID-er uten cacheinjeksjon eller overstyring av brukerens fabrikkdata.

21 utvalgte prosjektinnganger er identiske før og etter native-kjøringen.
Lokal source_commit er null fordi kildekoden ble bygget før commit;
filhashene identifiserer de faktiske inngangene. Dette er ikke full
kompilator-/systemheader-/JUCE-lukning. Binærfiler er hashkontrollert, men
ikke duplisert i dette Git-arkivet.

[Fersk Windows-bygging og faktisk installasjon](windows/README.md) består
for kilde7323545: 4/4 native-grupper og 197 av 198 Python-tester med ett
privilegierelatert symlink-hopp. PS/BAT-installasjon/erstatning/gjenoppretting
og innlastet installert modul består. Pakke-/kilde-/brukerdata-/modul-/logg-
bindinger er etterkontrollert. Den eksakte pakken er manuelt publisert som
c12c6c4588f9fd0b8e4b8f08d40f1683d17d7dde, uten nytt bygg. Faktisk HTTP200-
nedlasting har samme lengde og SHA-256. Ingen faktisk Ableton Live-kjøring
er utført her, og må ikke utledes fra testverten.
