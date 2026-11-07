# Flysnapper MIDI 0.4.2

Alle 52 akkordtyper og 38 skalaer beholdes. Fire fabrikkvalg endres:

| Akkord | Ny fabrikkskala |
| --- | --- |
| C7#9 | Half Whole Diminished |
| C7b13 | Melodic Minor 5: Mixolydian b6 (Hindu) |
| C13#9 | Chromatic |
| C13add#9 | Chromatic |

De to første rettelsene kommer fra den nye Excel-filen. De to siste
følger brukerens senere uttrykkelige valg i chatten og går foran filen.
Ingen akkorder, parametre, skalaer eller gjenkjenningsregler fjernes.
Det planlagte fargeunntaket er kansellert. Vanlig kommentering og farger
beholdes; Chromatic inneholder alle akkordtonene for de to C13-typene.

Nye instanser og **Reset all 52 to factory** bruker disse verdiene.
Lagrede instanser beholder sine egne tilordninger. Parameter-ID-er og
tilstandsformat er uendret.

Vertsrollen fra 0.4.1 beholdes: VST3-instrument med MIDI inn/ut, ingen
audioinngang og stille stereoutgang. Produktnavn og Fsm1-identitet er
uendret. Ingen audioanalyse, wrapper eller nytt pluginformat er innført.

[Brukerfil, senere valg og effektiv arbeidsbok](factory-settings/2026-11-07-user-values-final/README.md)
er bevart separat fra de tidligere fabrikkoppsettene. Ferske
[Windows-/installasjonskontroller](validation/midi-factory-final-2026-11-07/windows/README.md)
er bestått for kilde5269889. Den eksakte pakken er publisert uten ny
kompilering; faktisk nedlasting er kontrollert. Praktisk Ableton-kjøring
er ikke utført i skymiljøet.
