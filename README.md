# Flysnapper MIDI 0.4.2

0.4.2 bruker instrumentkategorien med en stille stereo lydutgang og MIDI
inn/ut. Dette er brukerens valgte neste prøve etter at Live 12.4.6 avviste
0.4.0 som audioeffekt uten audioinngang. Se
[diagnosen](docs/validation/live-open-failure-2026-11-07/README.md).

Flysnapper MIDI behandler bare MIDI og korrigerer etter skalaer
eller et akkordspor. Den stille instrumentutgangen endrer vertsrollen;
ingen audioinngang eller pitchgjenkjenning er lagt til. Ombyggingen følger brukerens bestilling
7. november 2026. De to nye Excel-rettelsene er C7#9 til Half Whole
Diminished og C7b13 til Mixolydian b6. Etter siste presisering får C13#9
og C13add#9 Chromatic som fabrikkvalg. Alle 52 akkordtyper og 38 skalaer
beholdes. Vanlig kommentering og farger er uendret.
Tidligere pakker og testbevis er bevart som egne kontrollpunkt. Den nye
instrumentrollen må kontrolleres i generisk VST3-vert og Windows-installasjon
før publisering. Praktisk aksept i Ableton gjenstår til brukerens prøve.

En ny Windows-pakke publiseres etter kontrollene; den gamle 0.4.0-pakken
har den bekreftede Live-feilen og er beholdt som historikk.

[Bestillingen](docs/MIDI_ONLY_REQUEST_2026-11-07.md),
[versjonsnotatet](docs/RELEASE_0.4.2.md) og
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
  dette bare når mottakeren trenger akkordkontrollen. La det være av ved
  sending til et lydinstrument, slik at akkordkontrollen ikke spilles.

## Skalaer og fabrikkvalg

38 skalaer: kromatisk; syv diatoniske moduser; syv moduser av melodisk moll;
tre bebopskalaer; syv moduser av harmonisk moll; syv moduser av harmonisk dur; to diminished; heltone;
durpentatonisk; mollpentatonisk; minor blues. Melodisk moll bruker samme
stigende tonemengde i begge retninger.

På baksiden viser hver av 52 rader akkordtoner og skalatoner med tonenavn.
Skalamenyen deler kompatible valg fra valg som utelater akkordtoner, med
overskriftene **Scales with all chord tones** og **Chord tones outside
scale**. Begge grupper følger parentfamilie og modusnummer. Uforenlige
valg er merket og navngir manglende toner; de er fortsatt valgbare.

[Brukerens fabrikkoppsett](docs/factory-settings/2026-11-07-user-values-final/README.md)
inneholder alle 52 valgene fra filen med de to etterfølgende Chromatic-valgene. Radene er
koblet etter stabile ID-er, og den opprinnelige arbeidsboken er bevart.
Hver ny instans bruker disse fabrikkvalgene. Eksisterende lagret tilstand
beholder sine egne tilordninger; Reset all 52 to factory bruker de nye
standardene. Skalanavn og skalatoner vises separat i pluginen.

## Format og vertsstatus

Den nye komponenten heter **Flysnapper MIDI** og bruker bundle
`Flysnapper MIDI.vst3`, med MIDI inn/ut, ingen audioinngang og én stille
stereo audioutgang. Identiteten fra 0.4.0 og alle parameter-ID-er er beholdt.
Egen pluginidentitet gjør at den gamle 0.3.0-komponenten og dens eksisterende
prosjekt-/automasjonsdata ikke blir erstattet. Gamle normaliserte
skalaparametre med 19 valg kan ikke uten videre brukes som 38-valgsautomasjon.

På et annet MIDI-/instrumentspor velges Flysnapper-sporet i øverste
**MIDI From**, og den pluginspesifikke **Flysnapper MIDI**-oppføringen i
feltet under. Mottakersporet trenger Monitor In, eller Auto med opptaksarm.
Dette er samme type MIDI-utgangsruting som brukeren tidligere fikk til.
0.4.2 er ikke en native MIDI-effekt som kan plasseres foran et annet
instrument på samme spor. Ingen Live-installasjon er tilgjengelig her;
den nye binærfilens praktiske Live-aksept gjenstår til brukerprøven.
CLAP, Max for Live og flere interne prosessorer er utsatt. Help og Guide
forklarer instrumentutgangen og den valgte rutingen.

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
Se [det aktuelle kontrollpunktet](docs/validation/midi-factory-2026-11-07/README.md).
Kildepakken beholder all aktuell kode og testdata; historiske DSP-arkiver
finnes i repositoriet og er utelatt fra den kompakte 0.4.0-kildepakken.

JUCE 8.0.16-security er låst til
`ea77444750de45e036e0279bb0291144bf8e3688`.
Lisens og opphav: [LICENSE.txt](LICENSE.txt), [THIRD_PARTY.md](THIRD_PARTY.md).
