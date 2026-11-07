# Effektive fabrikkvalg etter siste presisering

Alle 52 akkordtyper og 38 skalaer beholdes. Den opplastede filen er
bevart uendret som `Flysnapper_factory_assignments_user.xlsx` (18 640 byte,
SHA-256 `794a9941e128aaa3c1bc6f176c7d76a16e0dfc0344abf5624afac5654d0bb43c`).
`worksheet_assignments.json`/TSV registrerer filens to endringer fra
0.4.1: C7#9 → Half Whole Diminished og C7b13 → Mixolydian b6.

`assignments.json`/TSV er det effektive oppsettet. Her er brukerens senere
tekstvalg lagt til eksplisitt: C13#9 og C13add#9 → **Chromatic**. De
opprinnelige celleverdiene for disse to radene er registrert separat i
metadataene. Det er fire endringer totalt fra 0.4.1. Ingen andre valg,
akkord-ID-er, formler eller skaladefinisjoner endres.

Kanonisk mappinghash er
`8372f68334a1ed37e09500c0b527151a73fa6c18988fd0a972bf933bc3fd779e`.
Full metadata-JSON har egen hash; den må ikke forveksles med mappinghashen.

`Flysnapper_factory_assignments_effective_52_chords_38_scales.xlsx` er
en separat, kontrollert arbeidskopi med alle effektive verdier i kolonne
D og E. Den erstatter ikke den opplastede filen. Nye instanser og
**Reset all 52 to factory** bruker dette oppsettet; eksisterende instanser
beholder sine lagrede valg. Vanlig informasjon om skalatoner og farger
beholdes, uten spesialunntak.
