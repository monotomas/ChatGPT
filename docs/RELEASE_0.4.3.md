# Flysnapper MIDI 0.4.3

0.4.2 mistet retningshistorikken når forskjellige spilletoner overlappet,
og slo sammen nye anslag som ble korrigert til en allerede klingende tone.
0.4.3 retter begge forholdene, gir kromatisk gjennomslipp uten gjenkjent
akkord og ved stopp, og samler akkordoppsettet i fire kompakte kolonner.

Retningsregelen beholdes: ved lik avstand brukes de to foregående
ukorrigerte spilletonene, uten å ta med den aktuelle tonen. Historikken
beholdes nå ved sekvensielt legatospill og holdes separat for interne
kilder og MIDI-kanaler. Flere anslag på samme sample i samme strøm regnes
som tvetydige og bruker reservevalget oppover. Ingen framtidige noter
eller analyseforsinkelse brukes.

Et nytt spilleanslag som får samme utgående tone som en holdt tone,
sender Note Off etterfulgt av ny Note On på samme sample, med det nye
anslagets velocity. Opprinnelige input-/outputbindinger, FIFO og
referansetelling beholdes: en eldre tones senere Note Off skal ikke
avbryte en nyere tone som fortsatt holdes. Den siste bindingens slipp
avslutter tonen. Sustain og Panic følger samme eierskap. Routing-testens
syntetiske note beholder sin tidligere lave prioritet ved kollisjon.

Eksempel i C-dur: F–G–G# blir F–G–A også med legatooverlapp. B–A–G#
blir B–A–G. C# som korrigeres til D, fulgt av et nytt D-anslag mens
første tone holdes, gir et nytt D-anslag i stedet for én lang tone.

Når ingen akkord gjenkjennes, bruker begge akkordmodusene Chromatic.
Dette gjelder tomt akkordinput, én unik tone og uidentifiserte flertonersett.
Gjentatte/holdte noter beholder sine korrekte slippbindinger. En enkelt
tone vises som `G (single note)`; durtreklangen G B D som `G Major`.
Gjenkjenning og lagrede akkord-ID-er er uendret.

Eksplisitt stoppet transport gir Chromatic i alle tre moduser, mens lagret
modus, grunntone og skala beholdes. Ved avspilling brukes de valgte
innstillingene igjen. Manglende eller ugyldig transportinformasjon skal
ikke tolkes som stopp. Påbegynte toner får fortsatt korrekt Note Off.

Baksiden har fire kolonner: 14 durtyper, 12 dominanter, 12 dominanter og
14 moll-/power-/dim-typer. C5 står rett etter Cm. Dominantkolonnene deles
mellom C7b13 og C7sus4. Skalamenyen står under akkordnavnet, og alle
kolonnene er synlige i bredden ved minste størrelse 1040 × 760. Loddrett
rulling viser resten av radene. Toneforklaringer, fabrikkvalg og alle
52 × 38 menyvalg beholdes; visningsrekkefølgen endrer ingen parameter-ID.

Alle 52 akkordtyper, 38 skalaer og de endelige fabrikkvalgene fra 0.4.2
beholdes. Vertskategori, stille stereoutgang, Fsm1-identitet,
parameter-/tilstands-ID-er og MIDI-ruting er uendret. Brukeren har
bekreftet at 0.4.2-rutingen fungerer; dette er ikke en påstand om at
den nye binærfilen allerede er kjørt i brukerens Live-oppsett.

Alle fire native testgrupper bestod på Linux og Windows fra runtime-kilde
`1abedf68cfcfe08038246210bd5fa6c3d124c0b6`. Kjerne:2194622 assertions;
GUI:21516; prosessor:318633 kontroller/21 tilfeller; kompilert VST3:
22060 rapportkontroller/30 tilfeller. Staccato/legato, rå tie-tabell,
ordnede Off/On-par, slippbindinger, sustain, akkordfallback og transport
inngår. Windows Python:200 bestått og én forventet skip av201 totalt.

Windows CI42421340311 bestod også faktisk PS/BAT-installasjon,
erstatning/gjenoppretting og innlasting av installert modul. Den eksakte
CI-pakken ble publisert uten ny bygging. HTTP 200,38777911 byte og SHA256
`af22cef16f2d88d0642c7dfe21a7da93828b7070e425c7e84448b097ea8360d0`
er kontrollert mot den publiserte nedlastingen. Se
[Windows-bevis og lenke](validation/midi-articulation-0.4.3/windows/README.md)
og [Linux-bevis](validation/midi-articulation-0.4.3/linux/README.md).
