# Flysnapper MIDI 0.4.0

**Rapportert vertsfeil:** Ableton Live12.4.6 viser pluginen, men avviser
innlasting. Årsaken undersøkes; gjeldende pakke skal ikke omtales som
bekreftet brukbar i Live. Se [undersøkelsen](docs/validation/live-open-failure-2026-11-07/README.md).

Flysnapper MIDI er en ren VST3-prosessor for MIDI-korreksjon etter skalaer
eller et akkordspor. MIDI-only-ombyggingen følger brukerens bestilling
7. november 2026. Brukerens utfylte Excel-fil er innarbeidet: alle 52
fabrikkvalg følger filen, med 20 endringer fra malen. 38 skalaer beholdes.
Den ferske fabrikkutgaven består 4/4 native-grupper på Linux og Windows.
Python har 198 tester: Linux 196 bestått / 2 native BAT-hopp, Windows
197 bestått / 1 symlink-hopp. Faktisk Windows-installasjon, erstatning,
gjenoppretting og innlasting av installert VST3 består.
Tidligere foreløpige pakker og testbevis er bevart som egne kontrollpunkt.

[Last ned ferdig Windows x64-pakke](https://github.com/monotomas/ChatGPT/raw/c12c6c4588f9fd0b8e4b8f08d40f1683d17d7dde/downloads/Flysnapper-Windows-x64.zip).
Pakk ut hele ZIP-filen og kjør INSTALL_FLYSNAPPER.bat. Velg Flysnapper MIDI
i verten. Nedlastingen er kontrollert mot den bygde og installerte pakken.

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

38 skalaer: kromatisk; syv diatoniske moduser; syv moduser av melodisk moll;
tre bebopskalaer; syv moduser av harmonisk moll; syv moduser av harmonisk dur; to diminished; heltone;
durpentatonisk; mollpentatonisk; minor blues. Melodisk moll bruker samme
stigende tonemengde i begge retninger.

På baksiden viser hver av 52 rader akkordtoner og skalatoner med tonenavn.
Skalamenyen deler kompatible valg fra valg som utelater akkordtoner, med
overskriftene **Scales with all chord tones** og **Chord tones outside
scale**. Begge grupper følger parentfamilie og modusnummer. Uforenlige
valg er merket og navngir manglende toner; de er fortsatt valgbare.

[Brukerens fabrikkoppsett](docs/factory-settings/2026-11-07-user-values/README.md)
inneholder alle 52 valgene fra kolonne E i den opplastede filen. Radene er
koblet etter stabile ID-er, og den opprinnelige arbeidsboken er bevart.
Hver ny instans bruker disse fabrikkvalgene. Eksisterende lagret tilstand
beholder sine egne tilordninger; Reset all 52 to factory bruker de nye
standardene. Skalanavn og skalatoner vises separat i pluginen.

## Format og vertsstatus

Den nye komponenten heter **Flysnapper MIDI** og bruker bundle
`Flysnapper MIDI.vst3`, med MIDI inn/ut og null audiobusser. Egen
pluginidentitet gjør at den gamle 0.3.0-komponenten og dens eksisterende
prosjekt-/automasjonsdata ikke blir erstattet. Gamle normaliserte
skalaparametre med 19 valg kan ikke uten videre brukes som 38-valgsautomasjon.

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
Se [det aktuelle kontrollpunktet](docs/validation/midi-factory-2026-11-07/README.md).
Kildepakken beholder all aktuell kode og testdata; historiske DSP-arkiver
finnes i repositoriet og er utelatt fra den kompakte 0.4.0-kildepakken.

JUCE 8.0.16-security er låst til
`ea77444750de45e036e0279bb0291144bf8e3688`.
Lisens og opphav: [LICENSE.txt](LICENSE.txt), [THIRD_PARTY.md](THIRD_PARTY.md).
