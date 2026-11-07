# Andre Windows-CI-forsøk: Python-fixturefeil før native-bygg

[GitHub-kjøring 42381382145](https://github.com/monotomas/ChatGPT/actions/runs/42381382145)
gjaldt kildecommit `0a6772598a44609f2612b99c653217890efcf1ee` med 50 typer.
Den typede PowerShell-argumentrettelsen fungerte: status og logg ble publisert.

Python rapporterte 197 tester, seks feilforekomster og ett forventet
symlink-hopp på Windows. Feilene oppsto i den historiske audiogatens
isolerte protokollfixture: samme midlertidige fil ble sammenlignet som
lang Windows-sti og kort 8.3-alias. Dette er testfixturens stiidentitet,
ikke en ny kjørt pitchmotor eller grunnlag for å endre gamle lydkriterier.

MIDI-/pakke-/PowerShell-prøvene kjørte; native kompilering og pakking
startet ikke fordi Python-steget feilet. Ingen Windows-plugin ble bygget
i denne kjøringen. Logg/status er bevart byte-for-byte med inventarhashes.
Resultatet gjelder heller ikke brukerens senere 52-typeutvidelse.
