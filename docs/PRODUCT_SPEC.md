# Flysnapper – gjeldende produktoppdrag

Brukerens presiseringer 3. november 2026 går foran begrensningene i den
opprinnelige handoveren. Utvikle hele løsningen autonomt, og gjennomfør egne
bygginger, lydprøver, regresjonstester og installasjonskontroller. Ikke krev
brukertesting av delprototyper eller en bekreftet Ableton-prøve før DSP-utvikling.

Én Windows x64 VST3 på et MIDI-/instrumentspor er hovedløsningen. Audio mottas
på ekstern sidechain. En valgfri oppdeling i to plugins er bare et teknisk
alternativ hvis den gir en konkret fordel; den skal ikke bli en ny brukeroppgave.

## To funksjonsmoduler

Pitch-to-MIDI mottar audio og leverer ordinære Note On/Off, velocity og MIDI-note
på kanal 1. Mono/poly velges av brukeren. Modulen kan slås av uten å skade
samtidig eksternt MIDI. Ingen intern synthesizer eller dry-monitorering behøves.

MIDI-to-MIDI mottar generert og eksternt spille-MIDI samtidig. Kanal 16 er
akkordkontroll og lekker ikke til mottakerinstrumentet. Faktisk holdte noter,
inkludert noter som fortsetter over akkordskift, inngår i akkordanalysen.
Buffkillers korrigerte 50-typers bibliotek brukes for navn og numerisk tolking.

Brukeren kan velge en fast skala og grunntone, eller akkordstyrt skala fra
baksidens 50 uavhengige tilordninger. Chord Notes beholdes som direkte
akkordtonevalg fra handoveren. Alle 18 opprinnelige skalaer pluss Chromatic
beholdes, altså 19. Ingen kompatibilitetsfiltrering, skjult union eller familiearv.

Nærmeste tillatte MIDI-note velges globalt i 0–127. Ved lik avstand brukes
løpsretning og særreglene i handoveren, ellers oppover. Historikk gjelder
innnoter før korreksjon. Ingen framtidige noter ventes på. Nye noter bruker
akkorden ved sin faktiske beslutningstid; Note Off følger opprinnelig binding.
Holdt spille-note beholdes ved akkordskift. Tomt/ukjent akkordsett i akkordmodus
undertrykker nye noter. FIFO, kollisjonsreferanser og per-kilde sustain sikrer
avgrenset og dokumentert Note On/Off-adferd.

## Leveranse og bevis

Stor akkordvisning, effektivt Allowed Notes, mørkt Buffkiller-formspråk og flip
med alle 50×19 valg er del av produktet. Tilstand lagres per instans, og GUI må
ikke endre lyd-/MIDI-resultat eller beslutningstid.

En ferdig bygget Windows-installasjonspakke er ønsket leveranse. Brukeren skal
kunne pakke ut og installere uten Python, CMake, Visual Studio eller redigering
av kildekode. GitHub er godkjent for nedlasting. Ikke be om mellomliggende
produktprøver; løs utviklings- og pakkingsproblemer selv.

Målene om ekstremt lav audio-til-MIDI-latency står ved lag. Mål første korrekte
brukbare note, falske anslag, manglende toner og CPU-frister både på syntetiske
signaler og faktiske gitaropptak. E2<10 ms og E3<5 ms samlet kjede er brukerens
mål, ikke en egenskap som kan erklæres fra kompileringssuksess. Dokumenter
målte avvik i den endelige leveransen; ikke skjul lengre analysevinduer eller
fjern polyfoni som en umerket produktendring.
