# Flysnapper MIDI 0.4.0 – utviklingskandidat

Dette er ombyggingen til MIDI-only som brukeren bestilte 7. november 2026.
Den samlede Linux-native-kontrollen består 4/4 grupper. Python-kontrollen
har 198 tester: 196 består og 2 Windows-spesifikke BAT-prøver hoppes over
på Linux. Dette gjelder den nye utgaven med 38 skalaer. Windows skal ha
sin egen ferske 38/52-kontroll; de tidligere 31-skalaresultatene gjelder
et separat kildepunkt. Ingen ny brukerleveranse eller Live-godkjenning
er publisert.

Audio-/pitchmotor, sidechain, input dynamics, Learn og generert bending/MPE
er fjernet. Den nye modulen annonserer MIDI inn/ut og null audiobusser.
MIDI-korreksjon og melodiske retningsregler beholdes. De opprinnelige
50 akkordtypene er utvidet med CMaj13#11 og CMaj9#11 etter brukerens tillegg.
Eksternt uttrykk videresendes etter MIDI-kanalrollene.

Skalakatalogen har 38 valg. Harmonisk dur med alle sju bestilte moduser
er lagt til fra parentsettet 0, 2, 4, 5, 7, 8, 11. Menyene deler skalaer som inneholder alle
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
automasjon: 38 skalavalg har en annen normalisering enn 19 selv om de gamle
interne indeksene er bevart. Musikalsk XML-tilstand kan tolkes etter stabile
ID-er; det er ikke en garanti for automatisk overføring av gamle
prosjekters pluginautomasjon til den nye komponenten.

De opprinnelige 50 fabrikktilordningene er uendret; de to nye typene har
Lydian som foreløpig valg. Den
[utfyllbare Excel-malen](factory-settings/2026-11-07-revision-3/README.md) er gitt til
brukeren; ny fabrikkdata venter på den redigerte filen. Ikke oppgi dette
som ferdig innarbeidede nye brukerbestemte fabrikkvalg.

Kontrollresultater og faktisk kilde-/modulbinding legges i
`docs/validation/midi-only-38-scales-2026-11-07/`. Historiske audiofeil er beholdt som
historie og er ikke omtolket til beståtte MIDI-resultater.

De eksisterende 31 råindeksene og stabile ID-ene er bevart, med nye valg
på indeks 31–37. Normalisert automasjon fra tidligere, upubliserte
31-valgs forskningsbygg er ikke automatisk kompatibel med 38 valg.
Musikalsk XML-tilstand bruker stabile ID-er. Snapshot kapasitet og
avrunding av ferske valgverdier er tilpasset 38 skalaer og har egne
regresjoner; dette endrer ikke MIDI-korreksjonsreglene.
