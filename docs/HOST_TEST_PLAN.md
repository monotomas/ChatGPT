# Flysnapper MIDI0.4.0 – vertsverifikasjon

Dette er utviklerens kontrollplan for MIDI-only-utgaven, ikke en oppgave til
brukeren. Native VST3-testvert, Windows-installasjon og faktisk Ableton Live
må dokumenteres som forskjellige kjøringer. Pitch-/audioprøver hører til det
historiske oppdraget og er ikke med i denne modulens CMake-mål.

## Kjørt generisk native-kontroll

Byggingen kjører Core, PluginSmoke, MidiOnlyProcessor og ActualVst3. Den
faktiske kompilerte modulen må lastes og bestå følgende kontroller:

1. Navn Flysnapper MIDI, versjon0.4.0, egen Fsm1-komponentidentitet,
   Fx/Tools, null SDK-audiobusser og MIDI-eventbusser med16 kanaler inn/ut.
2. Kanal1 som spille-MIDI og16 som akkordkontroll. Ved felles sampleoffset
   behandles akkorddata før spilletoner uavhengig av innsettingsrekkefølge.
3. Alle31 skalavalg, grunntoner, korreksjonsmodi og50 map-parametre med
   stabile ID-er. Visuell menyrekkefølge må ikke bli parameterindeks.
4. Flere eksplisitt sammenkoblede instanser: kanal16 går gjennom mellom-
   instanser og konsumeres av siste. Flere aktive korreksjoner skal faktisk
   bruke forrige trinns utdata. En kasus som65→66→67 må skille dette fra
   behandling bare i siste trinn.
5. Intern Note Correction av/på under en holdt akkord; senere nye noter
   bruker oppdatert akkord uten ny akkordsending. Påbegynte spilletoner
   beholder sin opprinnelige Note Off-binding ved parameterendring.
6. Uttrykksdata, andre spillekanaler, pedaler, reset/transport, kopiering
   av instanstilstand, overbelastning og avgrenset opprydding.
7. GUI foran/bak/Guide, alle50×31 menykoblinger, vertens parameterendringer,
   skalavarsler/tonenavn, lesbarhet og ingen MIDI-endring bare ved GUI-bruk.

Bevar kildehashes, modul-/eksekverbarhashes, faktisk rå MIDI og rapporter.
Et lokalt null i source_commit er ikke en Git-attestasjon; de eksplisitte
filhashene beskriver den bygde arbeidskopien. Rapporter skillet mellom
utvalgte prosjektinnganger og full ekstern kompilator-/headerlukning.

## Windows-pakke og installert modul

CI skal bygge i Unicode-sti, kjøre de aktuelle native-/Python-testene og
pakke den nøyaktige bygde bundlen. Den utpakkede pakken skal installeres
privat og erstattes via de faktiske PowerShell- og BAT-inngangene.
Hashene i installert modul og gjenopprettingskopi må samsvare med pakken.
Last og kjør den installerte DLL-en i testverten etterpå. Både en uvedkommende
plugin og den gamle Flysnapper.vst3-komponenten skal forbli urørt.

Forskningskjøringer lagres isolert på validation/midi-only-ci. Den offisielle
nedlastingsgrenen må ikke oppdateres av denne forskningsflyten.

## Faktisk Ableton Live12.4.6 – gjenstående vertskontroll

Versjon og støtte for VST3-MIDI-kjeder er oppgitt av brukeren; det er ikke
en lokalt utført Live-test. Før en slik verifikasjon kan erklæres, må en
faktisk vertskjøring registrere nøyaktig Live-versjon, kilde-/modulhashes,
plassering av flere instanser på ett MIDI-spor og mottakerinstrument.

Avklar særlig:

- Om ruting til sporets inngang tilbyr og bevarer kanal1 og16 gjennom den
  aktuelle VST3-MIDI-kjeden.
- Hva vertsvalget «16 – Flysnapper» adresserer når flere instanser finnes;
  ikke anta at det adresserer alle parallelt eller at en bestemt instans
  velges uten at det er bekreftet.
- At vanlig Device Activator-bypass lar den ordinære strømmen fortsette,
  og separat hva som skjer med data som adresseres direkte til en instans.
- Hvilke holdte akkordnoter en reaktivert instans faktisk mottar. Manualens
  omtale av Chase MIDI Notes ved avspillingsstart er ikke et slikt bevis.
- At alle interne Note Correction-kombinasjoner og siste instans' konsum
  av16 virker med den faktiske rutingen og holder uvedkommende akkordnoter
  unna mottakerinstrumentet.

En driver som selv kobler instansers MidiBuffer sammen, eller eksplisitt
kaller processBlockBypassed, dokumenterer ikke disse Ableton-egenskapene.
Det er ikke tilgang til en slik Live-kjøring i det nåværende miljøet.
