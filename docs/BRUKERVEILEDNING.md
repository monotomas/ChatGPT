# Flysnapper 0.2.1 – brukerveiledning

Flysnapper gjør audio om til MIDI og korrigerer spille-MIDI etter en valgt
skala eller et akkordspor. De to modulene kan brukes sammen eller hver for seg.
Flysnapper lager ingen instrumentlyd selv; et mottakerinstrument spiller MIDI-en.

## Installasjon i Windows

Pakk ut **hele** Windows x64-pakken til en egen mappe, gjerne på D:. Windows 10
eller nyere, 64-bit, er målplattformen. Lukk
Ableton og andre pluginverter, og dobbeltklikk `INSTALL_FLYSNAPPER.bat`.
Velg VST3-mappen som Ableton bruker. Dette kan være Windows sin vanlige
VST3-mappe eller en egen VST3-mappe du allerede har aktivert i Ableton.
Pakken som er pakket ut, skal ligge utenfor denne pluginmappen.

Installeren kontrollerer filene og kopierer `Flysnapper.vst3` til valgt mappe.
Ved oppdatering beholdes en sikkerhetskopi av den gamle Flysnapper-bundlen
utenfor pluginmappen. Buffkiller og andre plugins blir ikke endret.
Python, CMake og Visual Studio trengs ikke. Kildearkivet under `Source/`
følger med for lisenskrav og videre utvikling.

Du kan også installere manuelt: Lukk Ableton, og kopier **hele den ytterste
`Flysnapper.vst3`-mappen**, med `Contents` og alt innhold, til VST3-mappen du
bruker. Standardplasseringen er `C:\Program Files\Common Files\VST3`. Ved
oppdatering flytter du først den gamle `Flysnapper.vst3` til en reservemappe
utenfor VST3-mappen. Kopier deretter inn den nye, komplette mappen. Den innerste
`.vst3`-filen alene er ikke hele pluginpakken.

Åpne Ableton etter installasjonen og la programmet skanne den valgte
VST3-mappen. Hvis mappen er en egendefinert VST3-mappe, må den være aktivert
i Abletons plugininnstillinger.

For å avinstallere lukker du pluginvertene og fjerner bare `Flysnapper.vst3`
fra pluginmappen. Ingen andre plugins eller Windows-innstillinger skal fjernes.

## Ruting i Ableton

1. Legg Flysnapper som instrument på et MIDI-spor. Vanlig spille-MIDI går inn
   på kanal 1. Et eksternt keyboard og audio-generert MIDI kan brukes samtidig.
   Ved livespilling settes Flysnapper-sporets monitorering til `In`.
2. Velg audio-kildesporet i Abletons sidechain-panel for Flysnapper. Slå på
   sidechain-inngangen der. Audio rutes inn her; det skal ikke rutes som
   vanlig audio-utgang til et MIDI-spor.
3. Lag et mottakerspor med instrumentet som skal spille. Velg Flysnapper-sporet
   som `MIDI From`, og velg selve Flysnapper-pluginens MIDI-utgang i den nedre
   velgeren. Sett mottakerens monitorering til `In`.
4. Hvis et akkordspor skal styre korreksjonen, sett akkordsporets `MIDI To`
   direkte til Flysnapper-pluginen og velg **kanal 16**. Denne rutingen settes
   manuelt på akkordsporet. Hele spille-inngangen skal fortsatt bruke kanal 1.

MPE må være av. Kanal 16 er reservert for akkordkontroll, og disse notene
sendes ikke videre til mottakerinstrumentet.

Knappen `Audio / MIDI routing` åpner denne rutingsforklaringen i Flysnapper.
Kilde- og mottakervalgene finnes i verten, utenfor pluginvinduet. En aktiv
sidechain-buss er ikke i seg selv bevis på at et bestemt kildespor er valgt.

## Audio til MIDI

Slå på `Audio to MIDI`. Velg `Monophonic` for én tone om gangen eller `Polyphonic` for
inntil seks samtidige toner. Detektoren bruker A4 = 440 Hz og MIDI-noteområdet
28–100. Den leverer vanlige MIDI-noter med velocity beregnet fra signalstyrken.

- `New note threshold` setter minste nivå for en ny detektert note. En høyere
  terskel avviser mer av bakgrunnsstøyen, men kan miste svake anslag. En allerede
  holdt tone kan ringe svakere ut under terskelen mens pålitelig periodisk
  toneinformasjon består.
- `Sensitivity` bestemmer hvor lett svak toneinformasjon godtas. Høyere verdi
  kan hjelpe svake toner, men kan også øke antallet feilanslag.
- `Release` bestemmer hvor lenge en tone får fortsette etter at toneinformasjonen
  forsvinner. Kortere verdi gir raskere slipp; lengre verdi tåler flere avbrudd.

Inngangsmåleren viser mottatt signal. De detekterte tonenavnene viser
toneklassene før korreksjon. Sidechain-status viser at verten har aktivert
inngangen; en aktiv inngang kan fortsatt motta digital stillhet.

Slå av `Audio to MIDI` for å bruke Flysnapper bare med eksternt MIDI.
Eksterne holdte noter beholdes når audio-modulen slås av.

### Bend og vibrato i monofonisk modus

Slå på `Mono pitch bend` for å gjengi en sammenhengende gitarbend eller vibrato
med vanlige MIDI-pitch-wheel-meldinger. Funksjonen er av som standard og er bare
tilgjengelig i `Monophonic`. I `Polyphonic` er kontrollene deaktivert; det lagrede
valget beholdes når du går tilbake til mono.

Velg `Range: +/-2 st` for inntil to halvtoner opp eller ned, eller `Range: +/-12 st`
for inntil en oktav. Bøyeområdet gjelder også mottakerinstrumentet. Flysnapper
sender standard MIDI RPN for pitch-bend-sensitivitet før den genererte noten.
Hvis instrumentet ignorerer dette, velger du samme område i instrumentet.
Et annet område hos mottakeren gir feil størrelse på bend og vibrato.

Med mono-bend bruker audio-genererte noter og pitch wheel **kanal 2**. Sett
mottakeren til kanal 2 eller alle kanaler. Vanlig spille-MIDI på kanal 1
beholder sin kanal og sine egne uttrykksmeldinger. Pitch wheel påvirker alle
noter på samme MIDI-kanal; ikke send annet spille-MIDI på kanal 2 hvis det skal
ha uavhengig bøying. Dette er vanlig MIDI og krever ikke MPE for én audio-stemme.

Kanalskillet gjelder ved Flysnappers MIDI-utgang. Videre ruting i Live kan slå
kanalene sammen, og enkelte instrumenter bruker felles pitch wheel for alle
stemmer også når meldingene har ulike kanaler. For uavhengige keyboard- og
audio-stemmer lar du keyboardet gå til sitt eget instrument og bruker en egen
mottaker bare til audio-notene. Kanal 2 alene garanterer ikke uavhengig uttrykk
gjennom hele verten og mottakerinstrumentet.

Ved mono uten bend og i poly sendes audio-genererte noter på kanal 1.
Kanal 16 er fortsatt akkordkontroll. Valgt kanal og forventet bøyeområde vises
nederst i Pitch-to-MIDI-panelet.

MIDI-korreksjonen velger den første uttonen ut fra skalaen eller akkorden.
Den sammenhengende bøyingen følger deretter gitarens tonehøyde omkring denne
uttonen. Slå av `Correction enabled` hvis bend skal følge den ukorrigerte tonen.

## MIDI-korreksjon

Fabrikkinnstillingen er `Fixed Scale` og `Chromatic`, slik at nye noter går
gjennom uten halvtoneendring inntil du velger en annen korreksjon.

- `Fixed Scale`: velg grunntone og en av de 19 skalaene, inkludert Chromatic.
- `Chord Notes`: bruk bare toneklassene i de holdte akkordnotene på kanal 16.
- `Chord Scale`: gjenkjenn akkordtypen og bruk skalaen som er valgt for denne
  typen på baksiden, transponert til akkordens grunntone.

`Chord / scale setup` åpner baksiden; `Back to playing` tar deg tilbake. Alle 50 akkordtyper har sitt eget skalaoppsett;
valg for én type endrer ikke de andre typene. Innstillingene lagres per
plugininstans sammen med Ableton-settet. `Reset all 50 to factory` setter alle
50 tilordninger tilbake til fabrikkvalgene.

`Allowed Notes` viser hvilke toneklasser som er tillatt. Akkordnavnet bygger
på Buffkillers akkordbibliotek. Noter som holdes på akkordsporet gjennom et
akkordskifte, inngår fortsatt i den nye analysen.

Korreksjonen velger nærmeste tillatte MIDI-note. Ved lik avstand brukes
retningen i et oppadgående eller nedadgående løp; oppover er reservevalget.
En holdt spille-note beholder uttonen den startet med, også når akkorden endres.
Nye noter bruker den nye akkorden. Tomt eller ukjent akkordsett i en av
akkordmodusene undertrykker nye noter til en gyldig akkord finnes.

Slå av `Correction enabled` for ukorrigert audio-til-MIDI og MIDI-gjennomgang.
`Panic` avslutter aktive noter og rydder holdpedaler hvis mottakeren henger.
Flip endrer bare visningen og avbryter ikke noter.

## Forsinkelse og måleresultater

Audio-til-MIDI trenger tid til å observere toneinformasjon. Pluginens rapporterte
0 latency-samples er ingen måling av denne deteksjonstiden eller av samlet
forsinkelse gjennom lydkort, Ableton og mottakerinstrument.

Pakkens `Validation/audio-validation/metrics.csv` viser første korrekte brukbare
note, feilnoter, manglende toner, slipp og CPU-tid i de automatiske lydprøvene.
`summary.json` beskriver de grunnleggende kvalitetssjekkene. Testene inkluderer
gitaropptak, men dekker ikke alle instrumenter, spilleteknikker eller lydmiljøer.

Denne versjonen oppfyller **ikke** målene E2 under 10 ms og E3 under 5 ms.
Audio-til-MIDI bruker typisk omtrent 45–60 ms før første riktige brukbare note
i målingene. Noen gitaranslag trenger nærmere 100 ms; enkelte krevende polyfone
stemmer bruker over et halvt sekund eller blir ikke gjenkjent. Poly kan også
legge til feiltoner. Svake grunntoner, overlappende oktaver og sterkt inharmoniske
lyder er kjente vanskelige tilfeller. Bruk Mono når inngangen skal tolkes som én
tone om gangen. MIDI-korreksjon av allerede innkommende MIDI venter ikke på
et slikt analysevindu.

Fullstendige Windows-målinger for alle tre lydsamlingene ligger under
`Validation/windows-audio-diagnostics/`: 729 + 621 + 414 kombinasjoner av lyd,
samplerate og blokkstørrelse. Rapportene beholder feil og CPU-fristbrudd som
observerte resultater. De 30 grunnleggende kvalitetssjekkene er en egen kontroll.
Målingene dokumenterer detektoren; en fysisk kjede gjennom lydkort, Ableton og
mottakerinstrument er ikke målt her.

For Poly er 64 eller 128 samples ved 48 kHz et mer forsiktig utgangspunkt enn
de minste bufferne. Windows-prøvene fant CPU-fristbrudd ved blant annet
32 samples / 96 kHz. Dette velges i vertens lydinnstillinger; mottakerinstrument
og resten av settet bruker også CPU-tid. Større buffer fjerner ikke tiden
detektoren trenger for å observere toneinformasjon.
