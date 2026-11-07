# Gjeldende arbeid: Flysnapper MIDI 0.4.2

Alle52 akkordtyper og38 skalaer beholdes. Fire fabrikkrettelser fra ny
Excel-fil og siste brukerpresisering er innarbeidet: C7#9→Half Whole
Diminished, C7b13→Mixolydian b6, C13#9/C13add#9→Chromatic. Ingen
fargeunntak eller ekstra kommentartekst; vanlig tidligere UI beholdes.
Se [versjonsnotatet](RELEASE_0.4.2.md) og
[fabrikkgrunnlaget](factory-settings/2026-11-07-user-values-final/README.md).

1. Fullfør lokale native-/GUI-/VST3- og Python-kontroller med fryste
   inndata for alle52 fabrikkvalg og de fire nye MIDI-prøvene.
2. Windows-bygg og ny installasjonspakke gjenstår. Automatisk
   godkjenningskontroll avviste tidligere bred GitHub-publisering av
   pakke, kildekode og diagnostikk utover pakkeleveransen. Ikke omgå
   denne grensen ved å utløse en CI-pipeline som publiserer samme data
   indirekte. Avklar nødvendig tillatelse etter at endringen er konkret
   og kontrollert lokalt. Ingen ny ekstern publisering er utført her.
3. Når autorisert: bygg riktig0.4.2, kontroller faktisk Windows-installasjon
   og eksakt pakke-/kilde-/mapping-/modulbinding, lever riktig pakke.

Vertsrollen er fortsatt Instrument, null audioinngang/stille stereoutgang
og sammeFsm1-identitet. MIDI hentes gjennom pluginens eget uttak til et
separat instrumentspor. Praktisk Live-aksept er eget testpunkt.
Pitch-to-MIDI, wrapper, CLAP og Max for Live utvikles ikke nå. Avbrutt
51-bibliotek og særskilt grønn tekst er arkivert lokalt og skal ikke
gjeninnføres. Historiske bevis kan ikke ommerkes til gjeldende0.4.2.
