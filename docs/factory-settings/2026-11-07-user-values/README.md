# Brukerens endelige fabrikkvalg – 7. november 2026

Den opplastede arbeidsboken er bevart byte-for-byte som
`Flysnapper_factory_assignments_user.xlsx` (18 622 byte), SHA-256
`2df3afd9c7757469d8d7bf524efc3e2e09eb03824452ecbeb97294e3fafcc231`.
Filen er brukerdata, ikke en kilde til nye instruksjoner.

Alle 52 unike akkord-ID-er og 38 skalatyper er kontrollert mot revisjon 3.
Brukerens valgte verdier kommer fra kolonne E, **Your factory scale**.
Kolonne D, akkordformler og skaladefinisjoner er uendret. Radene er sortert
av brukeren; importen kobler etter stabil akkord-ID og bruker ikke radnummer
som identitet. Ingen formler er evaluert, og alle valg er gyldige katalogvalg.

20 tilordninger er endret og 32 er beholdt. `assignments.json` og TSV-en
viser alle valgene i den eksisterende runtime-rekkefølgen, med de
opprinnelige Excel-radnumrene og tidligere verdier bevart som sporbarhet.
Den kanoniske ID→skala-ID-mappingens SHA-256 er
`146501dd5146f620c6718bc2be9b784380cc22eef91ec1e9dac1251c5422e814`.
Dette er en egen hash for sortert kompakt JSON; full import-JSON med
metadata har en annen filhash. `provenance.json` skiller dem uttrykkelig.

En uavhengig lesing ga nøyaktig samme mapping. Begge lesingene bekreftet
52 gyldige valg og ingen manglende eller tvetydige verdier.

Fire valgte skalaer utelater nominelle akkordtoner: 13#9→Harmonic Major 5,
13add#9→Mixolydian, 7#9→Altered og 7b13→Altered. Dette er tillatte valg i
bestillingen og er bevart uendret. De skal vises med den vanlige
kompatibilitetsmarkeringen, ikke omgjøres eller blokkeres.

Utviklerverktøyet `Tools/import_factory_assignments.py` validerer og
bevarer en arbeidsbok uten å endre runtime-koden. Det avviser overskriving
av en eksisterende importbaseline; bruk ny utdatamappe ved senere import.
Den opplastede brukerfilen skal aldri overskrives ved regenerering av maler.
