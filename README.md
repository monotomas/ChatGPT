# Flysnapper – musical-readiness research

Denne grenen inneholder uferdig utviklingsarbeid etter tilbakemeldingen
4. november. Den er ikke en ny, musikalsk godkjent installasjonspakke.
Status, uløste problemer og leveransesperre står i
[MUSICAL_READINESS.md](docs/MUSICAL_READINESS.md). Resten av denne siden
beskriver den tidligere 0.2.1-leveransen; dens validering godkjenner ikke den
nye lydanalysen.

Flysnapper er en Windows x64 VST3 med to moduler: audio til MIDI med mono-/polyvalg,
og MIDI-korreksjon til akkordtoner, akkordstyrt skala eller fast skala. Sidechain
tilfører audio; spille-MIDI kommer på kanal 1 og akkordkontroll på kanal 16.
Generert og eksternt spille-MIDI kan brukes samtidig.

## Tidligere Windows-pakke (0.2.1)

Last ned **Windows x64-installasjonspakken**, pakk ut hele ZIP-filen til en egen
mappe (for eksempel på D:), lukk Ableton og dobbeltklikk
`INSTALL_FLYSNAPPER.bat`. Velg VST3-mappen Ableton bruker. Python, CMake og
Visual Studio behøves ikke for denne pakken. Installereren kontrollerer filene
og beholder gjenopprettingsmulighet ved oppdatering. Buffkiller ligger separat.

Kildepakken er inkludert for videre utvikling og lisenskrav. Den er forskjellig
fra den ferdige Windows-pakken. `BUILD_FLYSNAPPER.bat` er et utviklerverktøy;
sluttbrukeren trenger ikke bygge pluginen.

## Bruk i 0.2.1

1. Legg Flysnapper som instrument på et MIDI-spor.
2. Velg audio-kilden i vertens eget sidechain-panel. Slå på `Audio to MIDI` og
   velg Mono eller Poly. New note threshold setter minste nivå for nye noter;
   holdte noter kan ringe svakere ut mens periodisk toneinformasjon består.
   Sensitivity styrer hvor lett svake tonekandidater godtas; Release
   bestemmer hvor lenge en mistet tone får fortsette før Note Off.
3. Eksternt keyboard-/track-MIDI kan samtidig gå inn som vanlig spille-MIDI.
   Både dette og detekterte noter bruker samme korreksjonsmodul.
4. Velg Fixed Scale med grunntone/skala, eller send akkordsporet direkte til
   Flysnapper-pluginen på kanal 16. Ikke filtrer hele spille-inngangen til kanal16.
5. I Chord Scale brukes baksidens valg for akkordtypen, transponert til akkordens
   grunntone. Chord Notes bruker bare de holdte akkordtonenes pitch classes.
   Alle 50 typer har egne valg med alle 19 skalaer. Flip endrer bare visningen.
6. På mottakersporet velges Flysnapper-pluginens MIDI-utgang som MIDI From.
   Et mottakerinstrument produserer lyden. Flysnapper har selv stille audio-utgang.

`Audio / MIDI routing` viser rutingen i pluginvinduet. Selve audio-kilden og
MIDI-mottakeren velges i verten: audio i Abletons Sidechain-panel utenfor
pluginvinduet, og MIDI via begge `MIDI From`-feltene på mottakersporet. Velg
Flysnapper-pluginen i det nederste feltet, og sett mottakerens monitorering til `In`.

`Mono pitch bend` følger kontinuerlige bend og vibrato i monofonisk modus.
Valget er av som standard. Når det er på, bruker audio-genererte noter og
pitch-wheel **kanal 2** ved Flysnappers MIDI-utgang. Eksternt spille-MIDI på
kanal 1 beholder sine meldinger der.
Velg bøyeområde ±2 eller ±12 halvtoner. Mottakerinstrumentet må motta kanal 2
(eller alle kanaler) og bruke samme område. Flysnapper sender standard MIDI RPN
for området; instrumenter som ikke følger denne meldingen, stilles manuelt.
Poly og vanlig mono uten bend bruker fortsatt kanal 1. Én audio-stemme med bend
trenger ikke MPE.
Videre ruting i verten eller et instrument med felles pitch wheel kan likevel
slå uttrykket sammen. For uavhengige keyboard- og audio-stemmer brukes et eget
instrument til keyboardet og en mottaker som bare spiller audio-notene.

Fabrikkoppsettet er Fixed Scale/Chromatic: nye noter går gjennom uten
halvtonekorreksjon inntil annen modus velges. Slå av Correction for ren
Pitch-to-MIDI eller vanlig MIDI-gjennomgang. Slå av `Audio to MIDI` for bare
keyboard-korreksjon. MPE må være av; kanal 16 er reservert for akkordkontroll.

Noter som holdes i akkordsporet, følger med gjennom neste akkordskifte. Holdte
spille-noter beholder den uttonen de startet; nye noter bruker den nye akkorden.
Tomt/ukjent akkordsett i akkordmodus undertrykker nye noter. Panic rydder
aktive noter og videreførte holdpedaler. Sidechain-status viser aktivert buss;
stillhet er ikke bevis på frakobling.

## Målinger og begrensninger

Audio-til-MIDI oppfyller foreløpig ikke målene E2 under 10 ms og E3 under 5 ms.
Første riktige brukbare tone kommer typisk etter omtrent 45–60 ms i prøvene;
enkelte tilfeller tar lengre tid. Polyfon gjenkjenning kan fortsatt legge til
feiltoner eller miste stemmer, særlig ved svake grunntoner og overlappende
oktaver. Full funksjonalitet i begge moduler betyr ikke feilfri transkripsjon
av vilkårlig audio. Vanlig MIDI-korreksjon trenger ingen ekstra observasjonsblokk.

Se `docs/AUDIO_VALIDATION.md` og `docs/TEST_RESULTS.md` for det som faktisk er
målt, inkludert feilnoter og første korrekte brukbare note. En Windows-kompilering
og en VST3-testvert dokumenterer ikke fysisk lydkort-/Ableton-latency. Rapporter
målt pitchforsinkelse separat fra pluginens rapporterte latency-samples.

## For utviklere

Gjeldende oppdrag står i `docs/PRODUCT_SPEC.md`; den opprinnelige handoverens
stoppinstrukser er erstattet av brukerens senere presiseringer. Referansen til
Buffkiller er beholdt uendret. 50 fabrikkvalg og noterutiner er dokumentert i
`Source/Core/README.md`.

Windows-kildebygg: Python 3.10+64bit (3.12 anbefales), Git, CMake og MSVC/Windows
SDK. Kjør `BUILD_FLYSNAPPER.bat`. JUCE 8.0.16-security er låst til eksakt commit
`ea77444750de45e036e0279bb0291144bf8e3688`; byggeren håndterer en separat
Unicode-tilpasset verktøykopi og kjører tester før eventuell installasjon.

Linux-sky: bruk eksisterende utsjekking; hver skyoppgave er allerede isolert,
så ikke opprett et Git-worktree uten uttrykkelig ønske. `bash Tools/cloud_setup.sh`
forbereder isolerte verktøy, og `bash Tools/cloud_validate.sh` bygger/tester med
midlertidig virtuell skjerm. Linux-binæren er en egen valideringsbinær.

`Tools/package_windows.py` krever dokumentert native Windows-bygg, ferske
beståtte tester, korrekte filhashes og integrert pitchmodul før installasjons-ZIP
kan lages. Den automatiske Windows-jobben bygger også i en sti med norske tegn.

Lisens og opphav står i `LICENSE.txt`, `THIRD_PARTY.md` og lydfixturenes README.
