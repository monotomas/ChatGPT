# Windows 0.4.4: kontrollert installasjonspakke

[Last ned Flysnapper MIDI 0.4.4](https://github.com/monotomas/ChatGPT/raw/3df90ba2fe131d70bfd07f35f7273fe01eaa5ad3/downloads/Flysnapper-Windows-x64.zip).
Lukk Live, pakk ut hele ZIP-filen og kjør `INSTALL_FLYSNAPPER.bat`.

- Runtime-kilde: `57240a38575f3d453a69b6c5d90a34aefb56a219`.
- Windows CI: `42427397276`, forsøk 1.
- Forskningskontrollpunkt: `807dcea7504ae7fd66da461efe6a7251446efdc7`.
- Offisiell leveranse: `3df90ba2fe131d70bfd07f35f7273fe01eaa5ad3`.
- ZIP: 38 777 466 byte, SHA256 `2ab65cc65cae384bd47bee39f107344d712c78c15f15dbfda1232a857c98ebb0`.
- Windows-modul: SHA256 `59797b7afac3fbabe4f5622f0225101ab5ac127d11bfcda5bc008af2a9131f8a`.

Alle fire native grupper bestod: Core 2 203 740 assertions; GUI 21 600;
Processor 318 633 kontroller/21 tilfeller/3 171 hendelsesrader; faktisk
VST3-rapport 22 060 kontroller/30 tilfeller/1 762 rader. Stdout og den
installerte vertsprøven teller også rapportskrivingskontrollen, altså
22 061. Windows Python: 200 bestått og én forventet symlink-skip av 201.

PowerShell-installasjon, BAT-start og innlasting av installert modul har
alle exit0. Prøvene dekker Unicode-stier, installasjon/erstatning og
gjenoppretting. Eldre Flysnapper og en annen plugin beholdes. Instrument-
metadata, stille stereoutgang og beholdt Fsm1-identitet er kontrollert.

`offline-byte-audit.json` binder ZIP, komplett innebygd kildepakke, alle
21 utvalgte prosjektfiler mot Git, modulmetadata, rå MIDI-rapporter/CSV,
fabrikkgrunnlag og installasjonslogger. De 52 fabrikktilordningene og
38 skalaene er uendret. Kanonisk mapping-SHA er
`8372f68334a1ed37e09500c0b527151a73fa6c18988fd0a972bf933bc3fd779e`.
Den kjente fulle private Live-loggen finnes ikke i leveransen.

Den identiske CI-pakken er publisert uten ny bygging. `delivery.json`
registrerer faktisk HTTP200, full lengde og samsvarende hash fra den
samme lenken som gis brukeren. `SHA256.json` kontrollerer dette arkivet.
Råfiler beholder opprinnelige Windows CRLF og BAT-pauseprompt.

Testeksekverbare er utelatt fra forskningsgrenen og dette tekstarkivet.
CI kontrollerte deres registrerte hasher før pakking; den lokale
bytekontrollen kan ikke rehashe eller kjøre de utelatte filene. ZIP og
kildearkiv er beholdt utenfor dokumentasjonsarkivet. Ingen faktisk
Ableton Live-kjøring av 0.4.4 påstås fra disse testvertene.
