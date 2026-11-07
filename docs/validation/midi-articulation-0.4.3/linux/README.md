# Linux-kontroll av 0.4.3

Alle fire native testgrupper bestod med 21 utvalgte kilde-/test-/byggfiler
identiske med Git-commit `1abedf68cfcfe08038246210bd5fa6c3d124c0b6`.
Den lokale manifest-generatoren skriver `source_commit: null` uten
CI-metadata. `summary.json` dokumenterer den separate sammenligningen med
Git-blobene; dette er ikke en hash av hele kompilator-/headeravhengigheten.

| Kontroll | Resultat |
|---|---|
| Kjerne | 2 194 622 assertions; 10 000 realtime-iterasjoner uten nye allokeringer |
| Faktisk prosessor og GUI | 21 516 assertions, inklusive alle52×38 menybindinger |
| Prosessorstrømmer | 318 633 kontroller,21 tilfeller,3 171 hendelsesrader |
| Kompilert VST3 | 22 060 rapporterte kontroller,30 tilfeller,1 762 hendelsesrader |
| Python | 199 bestått,2 forventede Linux-skip,201 totalt |

VST3-programmets stdout teller også sluttkontrollen ved rapportskriving,
altså22 061. Python-loggen kommer fra 2986b87; etterpå endret 1abedf6 bare
VST3-testens forventning om vertens kontrollmeldingsrekkefølge og
feildiagnostikk. Python-kode/tester er uendret. Windows kjører hele suiten
på den endelige kildeversjonen.

PNG-ene viser front, fire kolonner ved 1040×760 og 1500×1100, siste rader
og Guide. Kolonnene har 14/12/12/14 typer; C5 følger Cm og bruddet er
C7b13/C7sus4. De 52 typene,38 skalaene og fabrikkhash8372... er beholdt.

Pinned JUCE konverterer kontrollmeldinger til VST3-parameterkøer og
behandler dem før note-EventList på samme sample. Den faktiske vertstesten
kontrollerer denne konkrete rekkefølgen; direkte prosessor-/kjernetester
kontrollerer også kontrollmeldinger mellom og etter samtidige noter.
Ingen rå MIDI-rekkefølge mellom to separate VST3-domener påstås bevart.

Direkte SDK-prøve uten ProcessContext gir normal korreksjon. Et gyldig
stoppet ProcessContext gir Chromatic. JUCEs testvert kan selv syntetisere
gyldig stoppmetadata fra manglende informasjon oppstrøms; dette regnes
som vertens rapport, og merkes separat i testene. Pluginens offentlige
playhead-API kan ikke gjenfinne tapt opprinnelse til slike metadata.

Råfiler beholdes med opprinnelige bytes og SHA256-inventar. Testeksekverbare
filer og selve Linux-bundlen er utelatt fra dette tekst-/bildearkivet;
manifestene registrerer deres hasher fra kjøringen. Dette er Linux-bevis,
ikke Windows-installasjonsbevis eller en utført Ableton Live-prøve.
