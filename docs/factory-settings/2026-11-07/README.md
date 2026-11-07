# Excel-mal for Flysnappers fabrikkvalg

Åpne `Flysnapper_factory_assignments.xlsx`, rediger de gule cellene i
**Your factory scale**, lagre og last opp arbeidsboken i chatten. Behold
Chord ID-ene. Filen har alle50 akkordtyper og alle31 bestilte skalaer;
skalanavn kan velges fra nedtrekksmenyen. Nåværende fabrikkvalg er forhåndsutfylt
med de tilsvarende nye navnene, slik at uendrede rader er entydige.

`chord_types.tsv` kan kopieres rett til Excel. Skalaene er også i
`scale_catalog.json` og arbeidsbokens **Scales**-ark, ordnet etter den
bestilte parentfamilien og modusnummer. Modusintervaller er avledet fra
dur, melodisk moll og harmonisk moll; de tre bebopformlene følger teksten
ordrett. Alle tonemengder er relativt C.

Akkordtonene følger bibliotekets nominelle formel, inkludert toner som
gjenkjenningen kan tillate utelatt. Enharmoniske stavemåter kan forekomme;
Bbb i dim7 er samme tonehøyde som A. Dette er arbeidsdata, ikke en endring av
akkordbiblioteket eller en ny musikalsk korreksjonsregel.

Arbeidsboken er åpnet igjen og kontrollert:50 unike ID-er,31 skalaer,
50 uendrede forhåndsutfylte valg og nøyaktige bebopintervaller. Kildefilhashes
og datafilhashes finnes i `provenance.json`. Ingen runtime-/GUI-kode er
endret; ingen av de nye funksjonene er levert i en plugin ved dette punktet.
