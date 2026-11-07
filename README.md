# Flysnapper MIDI 0.4.0

Flysnapper MIDI er en ren VST3-prosessor for MIDI-korreksjon etter skalaer
eller et akkordspor. MIDI-only-ombyggingen følger brukerens bestilling
7. november 2026. Den er under teknisk verifikasjon; det er ennå ingen
publisert, praktisk Live-godkjent 0.4.0-leveranse. Den samlede Linux-native-
kontrollen for 52-typeutgaven består 4/4 grupper. Python-kontrollen har
198 tester: 196 består og 2 Windows-spesifikke BAT-prøver hoppes over på
Linux. Windows- og Live-resultater må dokumenteres separat.

[Bestillingen](docs/MIDI_ONLY_REQUEST_2026-11-07.md),
[versjonsnotatet](docs/RELEASE_0.4.0.md) og
[brukerveiledningen](docs/BRUKERVEILEDNING.md) beskriver gjeldende løsning.
Gamle pitch-/audioresultater er beholdt som historie; den modulen er fjernet.

## MIDI og korreksjon

- Spille-MIDI på kanal 1 korrigeres. Kanal 16 brukes til akkordkontroll.
  Andre spillekanaler videresendes uten notekorreksjon.
- **Chord Tones Only** bruker de holdte akkordtonene. **Chord-Scale
  Assignment** bruker skalaen som er tilordnet den gjenkjente akkordtypen.
  **Fixed Scale** bruker valgt grunntone og skala uavhengig av akkordsporet.
- Buffkillers 50 korrigerte typer, to brukerbestilte #11-tillegg og de melodiske
  retningsreglene er beholdt. Holdte noter følger med ved akkordskift.
- **Note Correction** av gir uendrede tonehøyder for nye spilletoner, mens
  akkordanalysen fortsetter. Påbegynte notebindinger, FIFO, kollisjons- og
  sustainregler beholdes slik at senere Note Off slipper riktig tone.
- **Chord MIDI Through** videresender kanal 16 til neste instans. Aktiver
  dette i mellominstanser og slå det av i siste instans, slik at
  akkordkontrollen ikke spilles av mottakerinstrumentet.

## Skalaer og fabrikkvalg

31 skalaer: kromatisk; syv diatoniske moduser; syv moduser av melodisk moll;
tre bebopskalaer; syv moduser av harmonisk moll; to diminished; heltone;
durpentatonisk; mollpentatonisk; minor blues. Melodisk moll bruker samme
stigende tonemengde i begge retninger.

På baksiden viser hver av 52 rader akkordtoner og skalatoner med tonenavn.
Skalamenyen deler kompatible valg fra valg som utelater akkordtoner, med
overskriftene **Scales with all chord tones** og **Chord tones outside
scale**. Begge grupper følger parentfamilie og modusnummer. Uforenlige
valg er merket og navngir manglende toner; de er fortsatt valgbare.

[Excel-malen](docs/factory-settings/2026-11-07-revision-2/README.md) har alle 52 stabile
akkord-ID-er og 31 skalaalternativer. Nåværende fabrikkvalg beholdes inntil
brukerens redigerte fil er mottatt og kontrollert. Hver instans lagrer sine
egne tilordninger med prosjekttilstanden. De to nye typene har Lydian som
foreløpig utgangspunkt. I Excel viser skalavalgene tonenavn i parentes;
i pluginen vises skalanavn og skalatoner separat.

## Format og vertsstatus

Den nye komponenten heter **Flysnapper MIDI** og bruker bundle
`Flysnapper MIDI.vst3`, med MIDI inn/ut og null audiobusser. Egen
pluginidentitet gjør at den gamle 0.3.0-komponenten og dens eksisterende
prosjekt-/automasjonsdata ikke blir erstattet. Gamle normaliserte
skalaparametre med 19 valg kan ikke uten videre brukes som 31-valgsautomasjon.

Brukeren oppgir Live 12.4.6 og støtte for VST3-MIDI-kjeder. Det er ikke utført
en faktisk Live-test i dette miljøet. Direkte ruting til én plugin,
ruting til sporets inngang og parallell levering er forskjellige ting.
[Vertsnotatet](docs/validation/midi-only-2026-11-07/host-routing/README.md)
skiller bekreftet kode-/manualgrunnlag fra uavklart vertsatferd.

Ved direkte ruting til første instans må den ikke deaktiveres i verten
under mottak; bruk intern Note Correction av/på. Ved sporruting som
bevarer kanal 1/16 kan den vanlige kjeden ha andre bypass-egenskaper.
Ingen automatisk omdirigering eller gjenutsending av holdte akkordnoter
ved reaktivering er forutsatt. Help og Guide forklarer kontrollene.

## Utvikling og kontroll

Bruk eksisterende checkout; ikke opprett Git-worktree uten uttrykkelig ønske.
I Linux-skyen:

```sh
bash Tools/cloud_setup.sh
source Tools/cloud_env.sh
CMAKE_BUILD_PARALLEL_LEVEL=2 bash Tools/cloud_validate.sh
python -m unittest discover -s Tests -p 'test*.py'
```

Valideringsskriptet eier `build/native-release/.flysnapper-build.lock` og
midlertidig Xvfb. Alle manuelle CMake/Ninja-kall, også diagnostikk, må bruke
samme lås. Ikke legg en ny lås rundt skriptet som allerede holder den.

Den aktuelle MIDI-utgaven har egne kjerne-, GUI-, prosessor- og faktiske
VST3-testvertprøver. Windows-pakking krever dessuten kilde-/modulbundet
bevis, PE-/Unicode-kontroller og reelle installasjons-/gjenopprettingsprøver.
En generisk testvert dokumenterer ikke faktisk Ableton-atferd.
Se [det aktuelle kontrollpunktet](docs/validation/midi-only-52-2026-11-07/README.md).
Kildepakken beholder all aktuell kode og testdata; historiske DSP-arkiver
finnes i repositoriet og er utelatt fra den kompakte 0.4.0-kildepakken.

JUCE 8.0.16-security er låst til
`ea77444750de45e036e0279bb0291144bf8e3688`.
Lisens og opphav: [LICENSE.txt](LICENSE.txt), [THIRD_PARTY.md](THIRD_PARTY.md).
