# Flysnapper 0.3.0 – brukerveiledning

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

La `MPE output` være av for den vanlige rutingen beskrevet her. MPE er et
valgfritt format for en mottaker som støtter det; det er ikke nødvendig for
vanlig mono/poly. Kanal 16 er reservert for akkordkontroll, og disse notene
sendes ikke videre til mottakerinstrumentet.

Knappen `Audio / MIDI routing` åpner denne rutingsforklaringen i Flysnapper.
Kilde- og mottakervalgene finnes i verten, utenfor pluginvinduet. En aktiv
sidechain-buss er ikke i seg selv bevis på at et bestemt kildespor er valgt.

## Audio til MIDI

Slå på `Audio to MIDI`. Velg `Monophonic` for én tone om gangen eller `Polyphonic` for
inntil seks samtidige toner. Detektoren bruker A4 = 440 Hz og MIDI-noteområdet
28–100. Den leverer vanlige MIDI-noter med anslagsstyrke i mono og estimert
styrke per stemme i poly. Dynamikkontrollene er ingen egen velocity-kompressor.
Poly bruker definerte halvtoner og ingen polyfoniske bendkurver.

- `Automatic input` bruker nivårelativ tone-/støyvurdering og brumreduksjon.
  Den er separat fra gaten på Input dynamics-siden og engangskalibreringen.
- `New note threshold` gjelder når Automatic input er av. En høyere
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

### Inngangsbehandling og 20-sekunders spilleprøve

Åpne **Input dynamics** og trykk **Learn / 20 seconds**. Demp strengene de
første **3 sekundene**, med vanlig håndkontakt og spillestilling; det hindrer
at endret jordingsbrum fra en urørt elgitar blir feil utgangspunkt. Spill
deretter i **17 sekunder** med svake/sterke anslag, vanlige akkorder og en
lang utklang. Følg fase og nedtelling. Verten må behandle audio for at prøven
skal gå framover. Genererte MIDI-toner er dempet under prøven; lytt til
gitarens lydspor. Eksternt MIDI og akkordkontroll fortsetter.

Et vellykket resultat stiller de synlige kontrollene og aktiverer behandlingen.
Du kan overstyre etterpå. Avbrudd eller ubrukelig prøve endrer ikke kontrollene;
en manuell justering under prøven avbryter den. Kalibrer på nytt etter en stor
endring i gitarvolum, pickup, lydkortgain eller støy. Eldre prosjekt/presets
beholder behandling av for kompatibilitet; bruk Learn eller aktiver den selv.

| Kontroll | Justering |
| --- | --- |
| Gate threshold | Høyere avviser mer støy og kan samtidig miste svake anslag. |
| Gate hysteresis | Lukkegrensen ligger dette antallet dB under åpnegrensen. Mer avstand bevarer mer utklang. |
| Gate attack | Kort tid åpner raskt og bevarer anslaget. |
| Gate hold | Holder gaten åpen gjennom korte nivåfall; for lang tid slipper inn mer støy mellom noter. |
| Gate release | Former dempingen etter hold. Lengre bevarer mer hale; kortere stopper støy tidligere. |
| Input gain | Trim før gaten; flytter både gitar og støy relativt til terskelen og forbedrer ikke signal/støy-forholdet. |
| Compressor | Terskel for reduksjon av sterke signaler, etter gaten. |
| Ratio | Styrken på kompresjonen over terskelen. |
| Limiter ceiling | Øvre nivå til gjenkjenningen; reparerer ikke klipping som allerede finnes i kilden. |

`Release` på Tracking-siden bestemmer MIDI Note Off etter mistet toneinformasjon;
`Gate release` former audio. Det er to forskjellige kontroller. Behandlingen
har ingen lookahead-buffer og øker ikke inngangsgain automatisk. En nivågate
kan ikke generelt skille en tone fra støy som klinger samtidig på lignende
nivå. Slik støy kan fortsatt nå detektoren når gaten står åpen.

**Help** gir forklaringer ved musepekeren på kontroller og etiketter.
**Guide** åpner en veiledning inne i pluginen, også med hoverhjelp avslått.
Innstillingene lagres sammen med prosjektet/presetet.

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

De tidligere absolutte E2/E3-grensene er erstattet av optimalisering og åpen
rapportering per tonehøyde. Fullkjedeprøver for inngangsbehandlingen ligger i
`Validation/input-dynamics/`, med faktiske MIDI-tidspunkter, anvendt oppsett,
kalibreringskontroller og feilutfall. De eldre detektorrapportene måler ikke
den nye dynamikkprosessoren. Ingen av prøvene måler den fysiske kjeden i
brukerens gitar, lydkort og Ableton-oppsett.

Svake grunntoner, nesten sammenfallende harmoniske kilder og sterkt
inharmoniske lyder er vanskelige tilfeller. Poly kan miste akkordtoner eller
legge til feiltoner. Bruk Mono for én tone om gangen. MIDI-korreksjon av
allerede innkommende MIDI venter ikke på et analysevindu.

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

Bruk lydkortets ASIO-driver på Windows. Prøv 64 eller 32 samples hvis hele
settet spiller stabilt; øk ved knepp/overbelastning. Ved 48 kHz varer én
64-samples buffer omtrent 1,33 ms, men flere ledd inngår i samlet responstid.
Unngå effekter med lookahead eller lange filtre foran sidechain-kilden.
Abletons `Reduced Latency When Monitoring` kan redusere kompensasjonsventing
på monitorerte spor; funksjonen fjerner ikke lydkortbufferen eller analysen.
96 kHz reduserer bufferens varighet ved samme antall samples og øker CPU-bruken;
det halverer ikke automatisk Flysnappers interne observasjonstid.
