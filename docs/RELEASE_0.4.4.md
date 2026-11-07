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
Kompilert GUI og VST3 samt Windows-installasjon kontrolleres før levering.
Resultater registreres i [kontrollpunktet](validation/midi-chord-display-0.4.4/README.md).
