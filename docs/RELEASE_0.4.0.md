# Flysnapper MIDI 0.4.0 – utviklingskandidat

Dette er ombyggingen til MIDI-only som brukeren bestilte 7. november 2026.
Den samlede Linux-native-kontrollen består 4/4 grupper. Python-kontrollen
har 197 tester: 195 består og 2 Windows-spesifikke BAT-prøver hoppes over
på Linux. Windows-verifikasjonen følger separat; dette er ikke en
publisert eller Live-godkjent leveranse.

Audio-/pitchmotor, sidechain, input dynamics, Learn og generert bending/MPE
er fjernet. Den nye modulen annonserer MIDI inn/ut og null audiobusser.
MIDI-korreksjon, akkordbiblioteket med 50 typer og melodiske retningsregler
beholdes. Eksternt uttrykk videresendes etter MIDI-kanalrollene.

Skalakatalogen har 31 valg. Menyene deler skalaer som inneholder alle
nominelle akkordtoner fra skalaer som utelater noen av dem. Begge grupper er
valgbare og følger parentfamilie-/modusrekkefølgen. Navngitte akkord- og
skalatoner vises, og akkordkolonnen beholder fast bredde ved vindusendring.
De nye modusnavnene er Chord Tones Only og Chord-Scale Assignment.

Note Correction kan slås av uten å stoppe mottatt akkordanalyse. Kanal 16
kan videresendes gjennom mellominstanser med Chord MIDI Through og
konsumeres av siste instans. Det er ikke automatisk kjedegjenkjenning.
Seriekoblingstester i en VST3-testvert er ikke en test av Live 12.4.6.
Se [rutingsgrunnlaget](validation/midi-only-2026-11-07/host-routing/README.md).

Den nye komponenten heter **Flysnapper MIDI**, med bundle
`Flysnapper MIDI.vst3` og egen pluginidentitet. Den gamle 0.3.0-komponenten
erstattes ikke. Dette beskytter gamle instrumentprosjekter og deres
automasjon: 31 skalavalg har en annen normalisering enn 19 selv om de gamle
interne indeksene er bevart. Musikalsk XML-tilstand kan tolkes etter stabile
ID-er; det er ikke en garanti for automatisk overføring av gamle
prosjekters pluginautomasjon til den nye komponenten.

Fabrikktilordningene er foreløpig uendret. Den
[utfyllbare Excel-malen](factory-settings/2026-11-07/README.md) er gitt til
brukeren; ny fabrikkdata venter på den redigerte filen. Ikke oppgi dette
som ferdig innarbeidede nye brukerbestemte fabrikkvalg.

Kontrollresultater og faktisk kilde-/modulbinding legges i
`docs/validation/midi-only-2026-11-07/`. Historiske audiofeil er beholdt som
historie og er ikke omtolket til beståtte MIDI-resultater.
