# Flysnapper: nytt oppdrag 7. november 2026

Den [originale innlimte brukerbestillingen](handover/Request_MIDI_only_2026-11-07.txt)
er gjeldende. Pitch-to-MIDI-arbeidet avsluttes. Hele audio-/pitchmodulen skal
fjernes fra produktet, med sine kontroller og kalibrering. Gamle resultater
beholdes som historie; ingen videre pitchforskning er bestilt.

Behold MIDI-to-MIDI: spille-MIDI på 1, akkordkontroll på 16, holdte akkordtoner,
fast skala eller akkordstyring og de eksisterende korreksjonsreglene. Behold
50 akkordtyper, uavhengig skalatilordning og lagring per instans.

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
- Ny tekst med fet skrift: «50 chord types, here assigned to root C. Your
  chosen scale type applies in the actual chord’s root and is auto-saved
  with this instance.»
- Følg med vanlig skrift: «Tool tip: If different assignments are required
  for different parts of the tune, save and copy/paste extra instances of
  Flysnapper to your track and automate each one on/off».

Brukeren vil endre factory assignments i Excel. En verifisert arbeidsbok med
50 stabile akkord-ID-er, navn/toner i C, eksisterende fabrikkvalg og 31
skalaalternativer er i [factory-settings](factory-settings/2026-11-07/README.md).
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
intern korreksjon er av. Dette krever ikke 50 nye akkordkanaler.

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
MIDI-only-ombyggingen er deretter implementert i kildecommit
`98b98f08ee5d561c013357784193e3be28319699`. Den gamle aktive Pitch-koden er
fjernet. Alle 4 aktuelle native-grupper består. Den siste Python-kontrollen
har 197 tester: 195 består og 2 native Windows-BAT-prøver hoppes over.
Windows-/installasjonskontrollen følger separat; dette er fortsatt ikke
en utført Live-test eller brukerleveranse.

Brukeren har siden oppgitt Live 12.4.6 og korrigert en viktig slutning:
Device Activator-bypass i sporets vanlige kjede beviser ikke omdirigering
av en feed som er routet direkte til én bestemt plugin. Brukeren foretrekker
kanal 16 til sporets inngang dersom denne kanalidentiteten bevares. Ikke lås
hjelpen til alltid aktiv første instans i alle oppsett; gjør forutsetningen
om direkte pluginruting tydelig. Se
[det avgrensede rutingsgrunnlaget](validation/midi-only-2026-11-07/host-routing/README.md).
