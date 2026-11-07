# Windows 0.4.3: bygget, installert og publisert

[Last ned ferdig installasjonspakke](https://github.com/monotomas/ChatGPT/raw/660d673c4358c20ad4ba09d32cab8dc1d699ec1f/downloads/Flysnapper-Windows-x64.zip).
Lukk Live, pakk ut hele ZIP-filen og kjør `INSTALL_FLYSNAPPER.bat`.

- Runtime-kilde: `1abedf68cfcfe08038246210bd5fa6c3d124c0b6`.
- Windows CI: `42421340311`, forsøk 1.
- Forskningskontrollpunkt: `16bc14608204bc93feabf7840944b000074c0ae0`.
- Offisiell leveranse: `660d673c4358c20ad4ba09d32cab8dc1d699ec1f`.
- ZIP: 38 777 911 byte, SHA256 `af22cef16f2d88d0642c7dfe21a7da93828b7070e425c7e84448b097ea8360d0`.
- Windows-modul: SHA256 `6d3ada2464a72b8ca656a409206fc4be90732ea930df8e630e2ec6ce8b0df70d`.

Alle fire native grupper bestod: Core: 2 194 622 assertions; GUI: 21 516;
Processor: 318 633 kontroller/21 tilfeller/3171 hendelsesrader; faktisk
VST3-rapport: 22 060 kontroller/30 tilfeller/1762 rader. VST3-stdout og den
installerte vertsprøven teller også rapportskrivingskontrollen, altså 22 061.
Windows Python:201 totalt,200 bestått og én forventet symlink-skip.

PS-installasjon, BAT-start og innlasting av installert modul har alle
exit 0. Prøvene omfatter Unicode-stier, installasjon/erstatning/gjenoppretting
og at eldre Flysnapper samt en annen plugin beholdes. Utgangen er målt
stille både ved vanlig og forbikoblet callback. Kategorien er Instrument,
ingen audioinngang, én stereoutgang og beholdt Fsm1-komponentidentitet.

`offline-byte-audit.json` binder ZIP, komplett innebygd kildepakke,
21 utvalgte prosjektfiler mot Git, rå MIDI-rapporter/CSV, modulmetadata,
fabrikkgrunnlag og installasjonslogger. Egne råstrømkontroller dekker
legato/staccato, samme-tone-anslag, kromatisk fallback, transport og
opprinnelige slippbindinger. Den direkte SDK-prøven skiller null
ProcessContext fra en gyldig stoppet kontekst; JUCEs syntetiske stopp
er merket separat. Se også Linux-notatet om kontrollkøenes rekkefølge.

Fabrikkens 52 tilordninger og 38 skalaer er uendret. Rå arbeidsbok,
effektiv arbeidsbok og chat-overstyringer ble verifisert byte for byte
mot endelig kilde. Kanonisk mapping-SHA er
`8372f68334a1ed37e09500c0b527151a73fa6c18988fd0a972bf933bc3fd779e`.
Den kjente fulle private Live-loggen finnes ikke i leveransen.

Den identiske CI-pakken ble manuelt flyttet til leveransegrenen uten
ny bygging. `delivery.json` dokumenterer den faktiske HTTP 200-nedlastingen,
full lengde og samsvarende hash. Det er samme lenke som gis brukeren.

Alle råfiler her beholder opprinnelige bytes, inklusive Windows CRLF og
avsluttende BAT-pauseprompt. `SHA256.json` kontrollerer arkivet. Windows-
testeksekverbare er utelatt fra forskningsgrenen og dette tekstarkivet;
hashene er registrert i manifestene og ble kontrollert av CI før pakking.
Den lokale bytekontrollen kan ikke kjøre eller rehashe de utelatte filene.
Selve ZIP-/kildearkivene er beholdt utenfor dokumentasjonsarkivet.

Dette er Windows-/VST3-/installasjonsbevis. Ingen faktisk Ableton Live-
kjøring av 0.4.3 påstås; brukerens bekreftede 0.4.2-ruting er beholdt.
