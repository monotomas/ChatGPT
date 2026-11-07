# Excel-ark med 52 akkordtyper

Bruk **Flysnapper_factory_assignments_52.xlsx**. Rediger de gule cellene i
kolonnen **Your factory scale** på arket **Assignments**, lagre og last opp
arbeidsboken i chatten. Behold akkord-ID-ene.

Revisjon 2 legger til:

| Chord ID | Navn i C | Akkordtoner |
| --- | --- | --- |
| major_thirteenth_sharp11 | CMaj13#11 | C D E F# G A B |
| major_ninth_sharp11 | CMaj9#11 | C D E F# G B |

Alle 31 skalavalg har nå skalatonene i parentes etter navnet, både i
nedtrekksmenyen og i de utfylte valgene. Eksempel: **Lydian (C D E F# G A B)**.
Arket **Scales** viser dessuten stabile skala-ID-er, intervaller og de
opprinnelige skalatypenavnene separat. Alle toner vises med C som grunntone;
tilordningen transponeres til den faktiske akkordens grunntone i pluginen.

De opprinnelige 50 akkord-ID-ene og skalatilordningene er beholdt. De to nye
radene har **Lydian** som foreløpig utgangspunkt, markert med cellekommentar.
Disse kan endres på samme måte som alle andre gule celler. Dette er en
redigeringsmal; nye fabrikkvalg fastsettes først fra brukerens redigerte fil.

Arbeidsboken er åpnet igjen og kontrollert: 52 unike akkord-ID-er, nøyaktige
toner for begge nye typer, 31 entydige skalavalg med tonenavn, uendrede valg
for de gamle 50 radene og gyldig nedtrekksmeny i E2:E53. `provenance.json`
binder de genererte filene og eksportverktøyet med SHA-256. Den opprinnelige
50-raders malen er bevart i nabomappen `2026-11-07`.

Utvikleren kan gjenskape arket med
`python3 Tools/export_factory_assignments.py` (krever openpyxl). Brukeren
trenger bare den ferdige XLSX-filen. Regenerering erstatter malen, så en
opplastet, redigert brukerfil skal alltid bevares separat.
