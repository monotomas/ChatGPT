# Flysnapper MIDI 0.4.4

«Chord tones» i spillevinduet viser nå akkordtonene i grunnstilling:
grunntone, ters eller sus-tone, kvint, eventuell sekst, septim, none,
11 og 13. Rekkefølgen følger den gjenkjente akkordens grunntone og
tonenes harmoniske rolle, uavhengig av omvending eller MIDI-oktav.

Eksempler:

- G Major: G B D, også når B eller D er laveste spilte tone.
- G13: G B D F A C E.
- C6/9: C E G A D.
- C7#9: C E G Bb D#.
- CMaj13#11: C E G B D F# A.

Bare faktisk mottatte toneklasser vises. Utelatt kvint eller andre
utelatte toner legges ikke til. Når flere navn er mulige, følger listen
den første gjenkjente tolkningen. Uidentifiserte tonesett beholder sin
tidligere toneklasseliste; en grunntone blir ikke gjettet for dem.

Hjelpetekst og Guide forklarer rekkefølgen. Endringen ligger i GUI-koden.
MIDI-prosessering, akkordgjenkjenning, de 52 akkordtypene, 38 skalaene,
fabrikkverdiene, lagrede innstillinger og instrument-/rutingrollen
beholdes fra 0.4.3. Firekolonneoppsettet er også beholdt.

Regresjonsprøvene dekker omvendinger, transponeringer, altererte
ekstensjoner, utelatte toner og at alle faktiske toneklasser bevares.
Alle fire native testgrupper bestod på Linux og Windows fra kilde
`57240a38575f3d453a69b6c5d90a34aefb56a219`: Core 2 203 740 assertions,
GUI 21 600, prosessor 318 633 kontroller og kompilert VST3 22 060
rapportkontroller. Den nye G13-visningen er også kontrollert visuelt.
De eksisterende rå MIDI-regresjonsstrømmene er byte-identiske med 0.4.3.

Windows CI `42427397276` bestod faktisk PS/BAT-installasjon,
erstatning/gjenoppretting og innlasting av installert modul. Python:
Windows 200 bestått og én forventet skip av 201; Linux 199 bestått og
to forventede skip. Den eksakte pakken er publisert uten ny bygging.
Nedlastingen ga HTTP200, 38 777 466 byte og samsvarende SHA256
`2ab65cc65cae384bd47bee39f107344d712c78c15f15dbfda1232a857c98ebb0`.
Se [resultater og nedlastingslenke](validation/midi-chord-display-0.4.4/README.md).
