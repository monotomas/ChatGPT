# Gjeldende arbeid: Flysnapper MIDI 0.4.1 instrumentadapter

Brukeren har valgt å prøve eksisterende Flysnapper som VST3 Instrument
med audioutgang før videre formatutvikling. Gjeldende leveranse er 0.4.1:
ingen audioinngang, stille stereo audioutgang, samme Fsm1-identitet og
samme `Flysnapper MIDI.vst3`-bundle som 0.4.0. Se
[versjonsnotatet](RELEASE_0.4.1.md) og
[oppsettet i brukerveiledningen](BRUKERVEILEDNING.md).

MIDI-algoritmen, 52 akkordtyper, 38 skalaer og brukerens 52 Excel-valg skal
beholdes. Det legges ikke til pitchgjenkjenning, audio-DSP eller sidechain.
Ingen wrapper-, Max for Live- eller CLAP-utvikling er valgt. Tidligere
planer om en ren MIDI-effekt og samme-spor-kjeding i Live er utsatt mens
instrumentadapteren prøves.

## Før pakken kan leveres

1. Fullfør ferske native kontroller av MIDI-reglene, alle 52 fabrikkvalg,
   38 skalavalg, instanstilstand og den faktisk kompilerte 0.4.1-modulen.
   SDK-busser og metadata skal vise Instrument, null audioinnganger og
   stille stereo audioutgang. Den nye adapteren skal ikke vurderes mot
   den gamle null-audiobusskontrakten.
2. Kontroller Windows x64-bygg og faktisk installasjon/innlasting av den
   installerte modulen. Bind riktig kilde, modul, brukerfil, mapping og
   installasjonsbevis til den eksakte 0.4.1-pakken før publisering.
3. Lever den kontrollerte pakken med konkret status for det som er prøvd.
   Praktisk Live-aksept er fortsatt uavklart: innlasting, kanal 1/16 og
   korrigert MIDI til et separat instrumentspor må bekreftes i Live.
   Øverste MIDI From velger Flysnapper-sporet; nederste velger pluginens
   MIDI-utgangspunkt; mottakersporet monitoreres.

## Etter den praktiske Live-prøven

Bruk resultatet fra denne instrumentadapteren før nye formatvalg tas.
Vellykket innlasting alene beviser ikke en MIDI-effektkjede foran et
instrument på samme spor. En eventuell videre bestilling om en slik
kjede må vurderes separat. Generiske testverter og Windows-kontroller
skal fortsatt ikke omtales som utført Ableton-testing.

0.4.0-resultater, den tidligere leveringen og gamle lydforsøk beholdes som
historie. De kan ikke ommerkes til bevis for 0.4.1. Brukerens fabrikkvalg
skal beholdes nøyaktig, også de fire valgene uten alle akkordtonene.
Konstruktøravrundingsregresjoner skal følge faktiske relevante
standardverdier uten cacheinjeksjon eller endring av brukerdata.
