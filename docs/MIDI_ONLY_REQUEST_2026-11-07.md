# Flysnapper: nytt oppdrag 7. november 2026

Den [originale innlimte brukerbestillingen](handover/Request_MIDI_only_2026-11-07.txt)
er gjeldende. Pitch-to-MIDI-arbeidet avsluttes. Hele audio-/pitchmodulen skal
fjernes fra produktet, med sine kontroller og kalibrering. Gamle resultater
beholdes som historie; ingen videre pitchforskning er bestilt.

Senere samme dag utvidet brukeren til **52 akkordtyper** og ba om skalatoner
i parentes etter alle skalatypenavn i Excel-valgene. Se
[tilleggsbestillingen](handover/Request_Chord_Additions_2026-11-07.txt) og
[den nye 52-raders arbeidsboken](factory-settings/2026-11-07-revision-2/README.md).
De to nye stabile typene er `major_thirteenth_sharp11` / CMaj13#11
(C D E F# G A B) og `major_ninth_sharp11` / CMaj9#11 (C D E F# G B).
De opprinnelige 50 ID-ene og tilordningene beholdes; de to nye får Lydian
som et foreløpig, redigerbart utgangspunkt. Skalanavn og tonenavn kan
fortsatt vises separat i pluginens grensesnitt.

Behold MIDI-to-MIDI: spille-MIDI på 1, akkordkontroll på 16, holdte akkordtoner,
fast skala eller akkordstyring og de eksisterende korreksjonsreglene. Behold
52 akkordtyper, uavhengig skalatilordning og lagring per instans.

- Bruk termene **Chord Tones Only** og **Chord-Scale Assignment**.
- Utvid til 31 skalaer: kromatisk; 7 diatoniske; 7 melodisk-mollmoduser; 3
  eksplisitt definerte bebopskalaer; 7 harmonisk-mollmoduser; 2 diminished;
  heltone;durpentatonisk;mollpentatonisk;minor blues. Bruk navnene og
  rekkefølgen i originalbestillingen.
- Del hver akkordtypes skalameny i «Scales with all chord tones» og
  «Chord tones outside scale». Tillat begge; advar/marker uten å blokkere.
  Sorter innen hver del etter parentfamilie og deretter modusnummer.
- Vis akkordtoner og skalatoner med navn. Lås akkordkolonnen til passende
  bredde; la den ikke vokse med hele vinduet.
- Ny tekst med fet skrift: «52 chord types, here assigned to root C. Your
  chosen scale type applies in the actual chord’s root and is auto-saved
  with this instance.»
- Hjelpeteksten følger brukerens senere rutingspresisering: automatér intern
  **Note Correction**, videresend kanal 16 i mellominstansene og konsumer
  den i siste instans. Kravet om aktiv første instans gjelder ved direkte
  pluginruting; det skal ikke framstilles som et krav i alle Track In-oppsett.

Brukeren vil endre factory assignments i Excel. En verifisert arbeidsbok med
52 stabile akkord-ID-er, navn/toner i C, foreløpige fabrikkvalg og 31
skalaalternativer er i [factory-settings](factory-settings/2026-11-07-revision-2/README.md).
Dette er en redigeringsmal. De nye skalaene og MIDI-grensesnittet er nå
implementert og har bestått den samlede native-kontrollen.
Ikke finn på nye fabrikktilordninger før brukerens redigerte fil foreligger.

## Instanser, vertsformat og avklaring før ombygging

Brukeren har Max for Live, men vil fortsatt ha **VST3-format**. Et separat
Kontakt-/instrumentspor er fullt aktuelt. Ønsket er flere seriekoblede
Flysnapper-instanser på ett MIDI-spor med MIDI-utgang på vertsnivå. Akkord-
sporet skal routes én gang til dette sporet på kanal 16, uten egne kanaler
eller vertsadresser for hver instans. Flere aktive instanser skal behandle
hverandres korrigerte spille-MIDI sekvensielt. Avslåtte instanser skal føre
spille-MIDI videre uten korreksjon.

Ved det opprinnelige kontrollpunktet annonserte pluginen Instrument Tools
med audioutgang og konsumerte kanal 16 før MIDI-utgang, også ved bypass.
Den nye komponenten annonserer Fx/Tools uten audiobusser. Chord MIDI Through
videresender kanal 16 mellom instansene, og akkordlytting fortsetter når
intern korreksjon er av. Dette krever ingen egen akkordkanal per instans.

Skillet mellom intern Correction on/off og vertens Device Activator må
håndteres: en vert som stopper prosesseringen, kan også stoppe akkord-
oppdateringer mens en instans er av. Det må også være tydelig hvordan
kanal 16 når alle instansene uten å bli uønskede toner i sluttinstrumentet.
Ingen automatisk kjedegjenkjenning er dokumentert.

Det er **ikke verifisert i faktisk Ableton Live** at en VST3 uten audiobusser
kan plasseres som Lives egne MIDI-effekter. Å fjerne DSP eller sette JUCEs
IS_MIDI_EFFECT-flagg er ikke i seg selv et slikt bevis. Generiske VST3-
testverter er andre verter. Den aktuelle offisielle Ableton-siden ble
blokkert av nettpolicy; en tidligere bevart sekundærkopi av Live 12-manualen
fra 2024 beskriver MIDI-effekter før instrumenter og VST/AU-instrumenter som
MIDI-til-audio, men beviser ikke den ønskede tredjepartsplasseringen.

Brukeren ba uttrykkelig om rask tilbakemelding før videre koding dersom
denne instans-/rutingsformen ikke er enkel å få til. Dette er meldt tilbake.
Ved det opprinnelige kontrollpunktet `d65f2bc` var ingen programkode endret.
Den første 50-type MIDI-only-ombyggingen er implementert i kildecommit
`98b98f08ee5d561c013357784193e3be28319699`. Den gamle aktive Pitch-koden er
fjernet. 52-typeutvidelsen er lagret i e891c2e og har
[ferske bevis](validation/midi-only-52-2026-11-07/README.md): alle 4 native-
grupper består på Linux og Windows. Python har 198 tester, med 196 bestått /
2 BAT-hopp på Linux og 197 bestått / 1 symlink-hopp på Windows. Faktisk
Windows-installasjon og innlasting av installert VST3 består. Dette er
fortsatt ikke en utført Live-test eller ferdig brukerleveranse.

Brukeren har siden oppgitt Live 12.4.6 og korrigert en viktig slutning:
Device Activator-bypass i sporets vanlige kjede beviser ikke omdirigering
av en feed som er routet direkte til én bestemt plugin. Brukeren foretrekker
kanal 16 til sporets inngang dersom denne kanalidentiteten bevares. Ikke lås
hjelpen til alltid aktiv første instans i alle oppsett; gjør forutsetningen
om direkte pluginruting tydelig. Se
[det avgrensede rutingsgrunnlaget](validation/midi-only-2026-11-07/host-routing/README.md).
