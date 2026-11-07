# Flysnapper: nytt oppdrag 7. november 2026

Den [originale innlimte brukerbestillingen](handover/Request_MIDI_only_2026-11-07.txt)
er gjeldende. Pitch-to-MIDI-arbeidet avsluttes. Hele audio-/pitchmodulen skal
fjernes fra produktet, med sine kontroller og kalibrering. Gamle resultater
beholdes som historie; ingen videre pitchforskning er bestilt.

Behold MIDI-to-MIDI: spille-MIDI på1, akkordkontroll på16, holdte akkordtoner,
fast skala eller akkordstyring og de eksisterende korreksjonsreglene. Behold
50 akkordtyper, uavhengig skalatilordning og lagring per instans.

- Bruk termene **Chord Tones Only** og **Chord-Scale Assignment**.
- Utvid til31 skalaer: kromatisk;7 diatoniske;7 melodisk-mollmoduser;3
  eksplisitt definerte bebopskalaer;7 harmonisk-mollmoduser;2 diminished;
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
50 stabile akkord-ID-er, navn/toner i C, eksisterende fabrikkvalg og31
skalaalternativer er i [factory-settings](factory-settings/2026-11-07/README.md).
Dette er en redigeringsmal; de nye skalaene/UI-et er ikke implementert ennå.
Ikke finn på nye fabrikktilordninger før brukerens redigerte fil foreligger.

## Instanser, vertsformat og avklaring før ombygging

Brukeren har Max for Live, men vil fortsatt ha **VST3-format**. Et separat
Kontakt-/instrumentspor er fullt aktuelt. Ønsket er flere seriekoblede
Flysnapper-instanser på ett MIDI-spor med MIDI-utgang på vertsnivå. Akkord-
sporet skal routes én gang til dette sporet på kanal16, uten egne kanaler
eller vertsadresser for hver instans. Flere aktive instanser skal behandle
hverandres korrigerte spille-MIDI sekvensielt. Avslåtte instanser skal føre
spille-MIDI videre uten korreksjon.

Bekreftet i eksisterende kode: pluginen annonseres som Instrument Tools med
audioutgang; kanal16 konsumeres før MIDI-utgang, også ved bypass. Den må
videresendes uendret mellom instansene, og akkordlytting bør fortsette når
intern korreksjon er av. Dette krever ikke50 nye akkordkanaler.

Skillet mellom intern Correction on/off og vertens Device Activator må
håndteres: en vert som stopper prosesseringen, kan også stoppe akkord-
oppdateringer mens en instans er av. Det må også være tydelig hvordan
kanal16 når alle instansene uten å bli uønskede toner i sluttinstrumentet.
Ingen automatisk kjedegjenkjenning er dokumentert.

Det er **ikke verifisert i faktisk Ableton Live** at en VST3 uten audiobusser
kan plasseres som Lives egne MIDI-effekter. Å fjerne DSP eller sette JUCEs
IS_MIDI_EFFECT-flagg er ikke i seg selv et slikt bevis. Generiske VST3-
testverter er andre verter. Den aktuelle offisielle Ableton-siden ble
blokkert av nettpolicy; en tidligere bevart sekundærkopi av Live12manualen
fra2024 beskriver MIDI-effekter før instrumenter og VST/AU-instrumenter som
MIDI-til-audio, men beviser ikke den ønskede tredjepartsplasseringen.

Brukeren ba uttrykkelig om rask tilbakemelding før videre koding dersom
denne instans-/rutingsformen ikke er enkel å få til. Dette er meldt tilbake.
Ved dette kontrollpunktet er **ingen programkode endret og ingen ny plugin
bygget**. Ikke påstå at ombyggingen eller en praktisk Live-test er ferdig.
