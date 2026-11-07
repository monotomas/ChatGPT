# Windows-verifikasjon: 38 skalaer / 52 akkordtyper

Kildecommit `76490afdb2971ef3de9860f1bb11c80070fb91ef`.
[CI-kjøring 42391529110](https://github.com/monotomas/ChatGPT/actions/runs/42391529110)
og immutable forskningscheckpoint
`4afba1914b9de931268ff9cf0172cc977aaa3f25` gjelder samme kilde.
Dette er intern teknisk verifikasjon, ingen offisiell brukerleveranse.

- Python: 198 totalt, 197 bestått og én symlink-prøve hoppet over på
  Windows fordi opprettelse kan kreve privilegier. Native BAT- og faktisk
  8.3-katalogalias-regresjon kjørte og besto.
- Native: alle 4 CTest-grupper består. Core har 2 191 245 kontroller,
  GUI 20 135, prosessor 107 320 / 14 grupper / 2 377 rå hendelser og faktisk
  VST3-modul 4 964 rapporterte kontroller / 21 tilfeller / 1 112 hendelser.
- Faktisk pakket PS/BAT-installasjon, erstatning og gjenoppretting i
  Unicode-stier med mellomrom og `&` består. Gammel instrumentkomponent
  og en uvedkommende plugin beholdes. Installer/BAT/innlastet installert
  modul returnerer 0; testverten har 4 965 kontroller inkludert rapportskriving.

ZIP: 39 274 041 byte, SHA-256
`5207a4689825a4e68fd4d7fb7d8845ca6ff963475b285951e2b1c3d2a334cdd2`.
Windows-DLL: SHA-256
`088385150d9405d91fbe305e9e7be1c87bc3de87525518745a38fb09c3e8477e`.

`offline-byte-audit.json` dokumenterer alle ZIP-/tilhørende kilde-ZIP-
medlemmer, 21 kompilerte prosjektinnganger mot eksakte 76490af-Git-blobs,
DLL/modulinfo, rårapporter/CSV/CTest og installert/gjenopprettet inventar
og loggbindinger. To testprogrammer er bevisst utelatt fra Git. Deres
registrerte hashes og rapportbindinger stemmer, men etterkontrollen kan
ikke selv rehashberegne eller kjøre utelatte programmer; CI rehashberegnet
dem før pakking. Store ZIP-er er hashregistrert og beholdt i checkpointet.

Den eksterne byggmanifestfilen har CRLF og den pakkede kopien LF.
JSON-verdier og normaliserte bytefølger er like; begge originale hashes
er bevart. Øvrige rapport-/kilde-/modul-/installasjonsbindinger er kontrollert
som eksakte bytes. Ingen produksjonsfil ble endret under etterkontrollen.

Audit-skriptet kom fra `build/windows-midi-attempt4/audit_checkpoint.py`.
For reproduksjon kopieres det tilbake dit og kjøres med checkpointet som
argument; prosjektroten beregnes fra denne plasseringen. Git-objektet må
være hentet. Resultatene skrives under samme build-mappe.

Dette er en faktisk Windows-/VST3-testvertkjøring, ingen faktisk Ableton
Live 12.4.6-kjøring. Plassering/ruting/kanalbevaring i Live er ikke verifisert.
Endelige fabrikkvalg venter fortsatt på brukerens redigerte 38-skala-Excel-fil.
