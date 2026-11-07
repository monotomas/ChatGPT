# Excel-ark med 52 akkordtyper og 38 skalaer

Bruk **Flysnapper_factory_assignments_52_chords_38_scales.xlsx**. Rediger
de gule cellene i **Your factory scale**, lagre og last opp arbeidsboken i
chatten. Alle skalavalg viser skalatonene i parentes etter navnet.

Harmonisk dur er lagt til med de sju modusene og nøyaktig navngiving fra
bestillingen. De vises etter harmonisk moll og før diminished-skalaene.
Alle toner her er relative til C:

| Skalatype | MIDI-halvtoner | Skalatoner |
| --- | --- | --- |
| Harmonic Major 1 | 0, 2, 4, 5, 7, 8, 11 | C D E F G Ab B |
| Harmonic Major 2 (Dorian b5) | 0, 2, 3, 5, 6, 9, 10 | C D Eb F Gb A Bb |
| Harmonic Major 3 (Phrygian b4) | 0, 1, 3, 4, 7, 8, 10 | C Db Eb Fb G Ab Bb |
| Harmonic Major 4 (Lydian b3) | 0, 2, 3, 6, 7, 9, 11 | C D Eb F# G A B |
| Harmonic Major 5 (Mixolydian b2) | 0, 1, 4, 5, 7, 9, 10 | C Db E F G A Bb |
| Harmonic Major 6 (Lydian Augmented #2) | 0, 3, 4, 6, 8, 9, 11 | C D# E F# G# A B |
| Harmonic Major 7 (Locrian bb7) | 0, 1, 3, 5, 6, 8, 9 | C Db Eb F Gb Ab Bbb |

Modusene er avledet ved å rotere parentsettet 0, 2, 4, 5, 7, 8, 11 og
sette hver ny grunntone til 0. Tonenavnene i de nye modusene følger
skalatrinnene: Fb tilsvarer E i MIDI, Bbb tilsvarer A, D# tilsvarer Eb og
G# tilsvarer Ab. Dette endrer ikke tonehøydene.

De tidligere 31 skalatypene, alle 52 akkord-ID-ene og de forhåndsutfylte
tilordningene er beholdt. De to sist tilføyde #11-akkordene har fortsatt
Lydian som foreløpig utgangspunkt. Ingen nye fabrikkvalg er valgt på
brukerens vegne; de skal komme fra den redigerte filen.

Arbeidsboken er åpnet igjen og kontrollert: 52 unike akkord-ID-er,
38 entydige skalavalg, gyldig nedtrekksmeny i E2:E53 og komplett navngitt
valgliste i Scales!C2:C39. Alle nye noteangivelser er sammenholdt med
MIDI-intervallene. `provenance.json` binder datafiler og eksportverktøy med
SHA-256. Tidligere regneark er bevart i revisjon 1 og 2.

Utvikleren kan gjenskape malen med `python3 Tools/export_factory_assignments.py`
(krever openpyxl). En redigert brukerfil skal alltid bevares separat og
skal ikke overskrives ved regenerering.
