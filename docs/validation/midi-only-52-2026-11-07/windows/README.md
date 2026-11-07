# Windows 52-typepakke – bestått teknisk kontroll

Kildecommit: `e891c2eef2f85e87138d22bbec8165e2b44f4c25`.
[CI-kjøring 42382978952](https://github.com/monotomas/ChatGPT/actions/runs/42382978952)
og endelig forskningscheckpoint
`74f91dc8c73cfa17bc088b247dda84202971175d` gjelder nøyaktig denne kilden.
Dette er et internt kontrollpunkt; offisiell brukerleveranse er ikke publisert.

- Windows-Python: 198 tester totalt, 197 består og én symlink-prøve hoppes
  over fordi opprettelse kan kreve privilegier. Den nye faktiske 8.3-alias-
  regresjonen kjørte og besto. Begge native BAT-prøvene kjørte.
- Native: alle 4 CTest-grupper består. Core har 2 142 807 kontroller,
  GUI 20 021, prosessor 87 182 / 12 grupper / 359 rå hendelser og faktisk
  VST3-modul 4 292 rapporterte kontroller / 21 tilfeller / 944 hendelser.
- Pakken ble installert og erstattet via de faktiske PowerShell-/BAT-
  inngangene i Unicode-stier med mellomrom og `&`. Gjenopprettingskopier,
  eldre instrumentkomponent og en uvedkommende plugin ble kontrollert.
  Installer, BAT og innlastet installert VST3 returnerte 0. Den installerte
  modulen besto 4 293 testvertkontroller inkludert siste rapportskriving.

Pakkens størrelse er 39 220 547 byte og SHA-256 er
`f0d2296037fa403a661f9fc807c9087f3a1649e780b6c776758e1277c9714e78`.
Den faktiske Windows-DLL-en har SHA-256
`931199be09a20bf0d145bb96aad7120b33d3b48ef474a4269f7c0ce1a9a4f9f6`.

`offline-byte-audit.json` beskriver etterkontrollen: alle ZIP-medlemmer,
tilhørende kilde-ZIP, de 21 utvalgte kildeinngangene mot Git-blobs i e891c2e,
DLL/modulinfo, rapporter/rådata/CTest og installert/gjenopprettet inventar
og logger ble kontrollert. Audit-skriptet er bevart; store ZIP-er er
hashregistrert og utelatt fra denne tekstkopien. De finnes i det uforanderlige
forskningscheckpointet. To testprogrammer er bevisst ikke lagret i Git;
etterkontrollen bekrefter deres registrerte bindinger, men kan ikke selv
rehashberegne eller kjøre utelatte programmer. CI rehashberegnet dem før pakking.

Den eksterne byggmanifestfilen har CRLF, mens den pakkede kopien har LF.
Første lokale byte-sammenligning stoppet på dette; stoppet er bevart.
JSON-verdier og de normaliserte bytefølgene er like, og begge opprinnelige
hashes beholdes. Rapport-/CSV-/CTest-/kilde-/modul-/installasjonsbindingene
er ellers kontrollert som eksakte bytes. Ingen produksjonsfil ble endret
for denne etterkontrollen.

Skriptet er bevart fra sin opprinnelige plassering i
`build/windows-midi-attempt3/audit_checkpoint.py`. For reproduksjon må det
kopieres dit fra dette arkivet og kjøres fra prosjektet med checkpoint-
argumentet over; det beregner prosjektroten fra den opprinnelige plasseringen.
Git-objektet må være hentet, og resultatet skrives under samme build-mappe.

Disse resultatene dokumenterer Windows-pakken og den eksplisitte
VST3-testverten. De dokumenterer ikke faktisk Ableton Live 12.4.6-plassering,
Track In-kanalbevaring eller Device Activator-adferd. ableton_verified er
fortsatt false. Nye brukerbestemte fabrikkvalg venter på Excel-filen.
