# Windows-pakken med brukerens fabrikkvalg

Kilde `7323545b2eda5a877a16a42248fea5c4e6cdc953` ble bygget i
[CI-kjøring 42398359144](https://github.com/monotomas/ChatGPT/actions/runs/42398359144).
Forskningscheckpoint `dcf66fdd2a31da595c1013a4431c14cfad2833e6` og endelig
leveringscommit `c12c6c4588f9fd0b8e4b8f08d40f1683d17d7dde` binder samme
verifiserte ZIP. Publiseringen var en manuell promotering av eksakte bytes,
uten nytt bygg. CI-feltene i publisert status identifiserer det faktiske
kildebygget, ikke en ny CI-kjøring.

- Python: 198 totalt, 197 bestått og ett privilegierelatert symlink-hopp.
- Native: 4/4 CTest-grupper består. Core 2 191 256 kontroller, GUI 20 293,
  prosessor 109 494 / 14 grupper / 2 379 rå hendelser, faktisk VST3-modul
  5 373 rapporterte kontroller / 22 tilfeller / 1 120 hendelser.
- Faktisk PS/BAT-installasjon, erstatning, gjenoppretting og innlasting av
  installert VST3 består. Den installerte testverten har 5 374 kontroller
  inkludert siste rapportskriving. Alle exitkoder er 0. Unicode-stier,
  gammel instrumentkomponent og en uvedkommende plugin er kontrollert.
- Siste CI-steg, inkludert opplasting og opprydding, er bestått.

Pakken er 39 302 326 byte, SHA-256
`aefa9ece3fcb0194c04f0729085285dce890611a030b8b2f6e08c1ebe830ee47`.
Windows-DLL SHA-256:
`f4567bc474dca5e857cd2286046689ba34219d527cd2eb944c557fa5c5e14b34`.

Etterkontrollen binder original bruker-XLSX, full import-JSON og den
kanoniske 52-ID-mappingen separat. Den sjekker den innbakte kildepakken,
21 eksakte kildeinnganger mot Git-blobs, modul-/rapport-/CSV-/CTest-bytes,
brukerfabrikkfunksjonen og hash i faktisk modulrapport, samt installert/
gjenopprettet inventar og logger. Brukerens 20 endringer er bevart nøyaktig.

Eksternt Windows-byggmanifest har CRLF, den pakkede kopien LF; JSON og
normaliserte bytes er like. Begge originale hashes beholdes. To native
testprogrammer er bevisst utelatt fra Git: CI rehashberegnet dem før pakking,
mens etterkontrollen bare kan bekrefte registrerte bindinger og ikke
rehashberegne eller kjøre utelatte programmer. Store ZIP-er er beholdt i
checkpointene og hashregistrert, ikke duplisert i dette tekstarkivet.

Audit-skriptets opprinnelige plassering er
`build/windows-midi-factory/audit_checkpoint.py`. Kopier det tilbake dit
for reproduksjon med forskningscheckpointet som argument; Git-objektene
må være hentet. Resultater skrives under samme build-mappe.

`delivery.json` registrerer den endelige nedlastingslenken. En faktisk HTTP-
nedlasting returnerte 200, riktig lengde og nøyaktig samme SHA-256 som CI-
pakken. Dette dokumenterer Windows-installasjon og VST3-testverten; faktisk
Ableton Live 12.4.6-plassering og kanalbevaring er fortsatt ikke kjørt her.
