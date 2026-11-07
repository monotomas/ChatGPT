# Fullført: 0.4.3 korreksjon og fire kolonner

Brukeren bekreftet fungerende ruting i 0.4.2, men meldte tapt legatoretning
og undertrykte nye anslag ved samme korrigerte tone. Se
[bestillingen](ARTICULATION_REQUEST_2026-11-07.md) og
[versjonsnotatet](RELEASE_0.4.3.md).

Opprinnelig to-foregående-råtoner-regel beholdes. Historiestrømmer er
separate; vanlig overlapp sletter ikke melodihistorien. Samtidige flere
anslag i samme strøm bruker reserve oppover uten lookahead. Nye
spilleanslag reartikulerer allerede aktiv måltone med Off/On samme
sample/ny velocity, mens gamle bindinger/FIFO/siste slipp beholdes.
Diagnostic-kollisjoner beholder lav prioritet.

Brukeren har også valgt kromatisk gjennomslipp i begge akkordmoduser når
akkorden ikke gjenkjennes, og i alle tre moduser ved eksplisitt stopp.
Kontrollparametere og eksisterende Note Off-bindinger beholdes. Ukjent
transport må ikke behandles som stopp. Navnene er `G (single note)` og
`G Major`. Godkjent tabell har fire kolonner14/12/12/14, C5 rett etter Cm,
og brudd mellom C7b13 og C7sus4. Alle fire skal passe ved1040×760.

Alle 52 akkordtyper/38 skalaer, fabrikkhash8372..., instrumentrolle,
Fsm1, parametre/lagring og vertsruting beholdes. Krev ferske Core-,
prosessor- og faktisk VST3-prøver av legato/staccato over blokkgrenser,
rå tie-tabell, flere kilder, nytt anslag, slipperekkefølger, sustain og
reset/overløp. Begge rapporter må bevise legato_directional_ties og
same_pitch_rearticulation, unrecognized_chord_chromatic_fallback og
stopped_transport_chromatic_fallback. Oppdater gamle sammenslåingsforventninger,
men behold urelaterte regresjoner.

Alle fire native testgrupper bestod på Linux og Windows. Den faktiske
Windows-pakken bestod PS/BAT-installasjon, erstatning, gjenoppretting og
innlasting av installert modul. Den eksakte CI-pakken er publisert og
nedlastingen kontrollert med HTTP200, lengde og SHA256. Se
[Windows-resultatene](validation/midi-articulation-0.4.3/windows/README.md).
Ingen utviklings-/byggjobb gjenstår. Fortsett med brukerens neste resultat
eller bestilling; ikke kjør brede suiter på nytt uten ny kode eller
uavklarte feil. Faktisk ny Live-prøve er ikke utført av testvertene.

Tidligere uttrykkelig godkjenning dekker GitHub-kode, Excel-grunnlag og
byggelogger. Ikke be om samme tillatelse igjen innenfor denne leveransen.
Fulle private Live-logger er fortsatt ikke godkjent for publisering.
