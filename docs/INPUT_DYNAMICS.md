# Inngangsbehandling og spilleprøve i Flysnapper 0.3.0

Flysnapper behandler sidechain-signalet med gate/expander, kompressor og
limiter før tonegjenkjenningen. Den automatiske brumreduksjonen får høre
inngangen før gaten demper den. Behandlingen har ingen lookahead-buffer.
Gategrensen bestemmer hvilke svake lyder som får starte nye toner; en lavere
lukkegrense og justerbar hold/release lar en etablert streng klinge videre.

## Første spilleprøve

1. Bruk et rent gitarsignal og vanlig gitarvolum, pickup og lydkortgain.
   Velg kilden i Abletons eget sidechain-panel. Unngå klipping ved inngangen.
2. Åpne **Input dynamics** og trykk **Learn / 20 seconds**.
3. Demp strengene i de første **3 sekundene**, med normal håndkontakt og
   spillestilling. Å fjerne hendene helt kan endre jordingsbrum på en elgitar.
4. Spill representativt i **17 sekunder**: svake og sterke anslag, vanlige
   akkorder og minst én lang utklang. Følg fase og nedtelling i vinduet.
5. Ved vellykket prøve flytter Flysnapper de faktiske kontrollene til de
   foreslåtte verdiene og aktiverer inngangsbehandlingen. Juster dem fritt
   etterpå. Gjenta prøven etter vesentlige endringer i signal eller støynivå.

Genererte MIDI-toner er dempet under prøven. Lytt til den opprinnelige gitaren
fra lydsporet. Eksternt spille-MIDI og akkordsporet fortsetter som vanlig.
Verten må behandle audio for at nedtellingen skal gå. Avbryt, manglende
spilling, utilstrekkelig nivåforskjell eller betydelig klipping gir ingen
automatisk endring. En manuell justering under prøven avbryter den, slik at
et senere resultat ikke overskriver justeringen. Endret tracking-oppsett eller
et transportavbrudd kan også avbryte prøven.

## Kontrollene

| Kontroll | Virkning og avveining |
| --- | --- |
| Input gain | Trim før gaten. Flytter både gitar og eksisterende støy relativt til terskelen; forbedrer ikke signal/støy-forholdet. Kalibrer på nytt etter en stor endring. |
| Gate threshold | Nivået som åpner for nye toner. Høyere verdi avviser flere smålyder, men kan miste svake anslag. |
| Gate hysteresis | Hvor mange dB under åpnegrensen gaten kan forbli åpen. Større avstand bevarer mer utklang og slipper samtidig gjennom mer støy i halen. |
| Gate attack | Hvor raskt gaten åpner. Kort tid bevarer anslaget og gir minst ekstra forsinkelse. |
| Gate hold | Hvor lenge gaten holdes åpen etter at signalet faller under lukkegrensen. Motvirker hakking rundt terskelen. |
| Gate release | Hvor raskt den gjenværende lyden dempes etter hold. Lengre tid bevarer mer hale; kortere tid stopper støy tidligere. |
| Compressor | Terskelen der sterke signaler reduseres. Kompressoren står etter gaten og løfter ikke støygulvet automatisk med makeup-gain. |
| Ratio | Hvor sterkt nivåene over kompressorterskelen reduseres. Kraftig kompresjon kan svekke ansatsinformasjonen. |
| Limiter ceiling | Øvre nivå inn til tonegjenkjenningen. Limiteren kan ikke rette opp klipping som allerede finnes i kilden. |

**Release** på Tracking-siden er separat: den bestemmer hvor lenge en mistet
tone får vente før MIDI Note Off. Gate release former audioen; MIDI Release
bestemmer livsløpet til en gjenkjent note. Lengre MIDI Release kan hjelpe
ved korte analysebrudd, men forsinker også avslutning ved virkelig demping.
Monofonisk velocity beholder anslagsstyrken før dynamikkbehandlingen; i poly
anslås styrken per gjenkjent stemme. Kontrollene former analyse-audio og er
ikke en egen MIDI-velocity-kompressor. Inngangsmåleren viser rått sidechain-
nivå; dynamikksiden viser faktisk behandlet nivå, gain og gatestatus.

**Automatic input** gjelder pitchmotorens nivårelative tone-/støyvurdering
og brumreduksjon. Det er forskjellig fra både gaten og engangskalibreringen.
**Help** slår forklaringer ved musepekeren av/på; **Guide** åpner veiledningen
inne i pluginen. Innstillingene lagres med prosjektet/presetet.

En nivågate kan ikke generelt fjerne støy som klinger samtidig med en ønsket
tone og har sammenlignbart nivå. Økt inngangsgain løser heller ikke dette.
Hvis en knirkelyd ligger over åpnegrensen, kan den fortsatt nå pitchmotoren.
Terskel, hysteresen og release må derfor avveies mot de svakeste tonene og
lengste utklangene man ønsker å beholde.

## Mono, poly og ruting

Mono har fortsatt valgfri bending. Poly sender definerte halvtoner med
Note On/Off og bruker ingen polyfoniske bendkurver. Begge går videre til
samme MIDI-korreksjon. Akkordkontroll er fortsatt inngangskanal 16 og blir
ikke sendt videre som spilletoner.

MPE er et valgfritt utgangsformat. Det er ikke nødvendig for vanlig poly
eller den eksisterende monofoniske kanal-2-bendingen. MPE krever støtte og
korrekt oppsett hos vert/mottaker; å velge det aktiverer ikke polyfonisk
bending. Vanlig ruting er den brukeren allerede har bekreftet i Ableton:
audio velges i vertens sidechain-panel, og mottakersporets nederste MIDI From-
felt velger selve Flysnapper-pluginens MIDI-utgang.

## Praktisk lavere forsinkelse

* På Windows: bruk lydkortets ASIO-driver og prøv 64 eller 32 samples dersom
  maskinen spiller stabilt uten knepp. Øk bufferen igjen ved overbelastning.
* Ved 48 kHz varer én 64-samples buffer omtrent 1,33 ms. Dette er bare én
  buffer, ikke hele forsinkelsen gjennom lydkort, analyse, MIDI og instrument.
* Bruk et rent sidechain-signal uten lookahead-kompressor, linear-phase-filter
  eller andre effekter som legger inn ekstra venting før Flysnapper.
* I Ableton kan **Reduced Latency When Monitoring** redusere kompensasjons-
  venting på monitorerte spor. Det fjerner ikke lydkortbufferen eller tiden
  som trengs til selve tonegjenkjenningen.
* Høyere samplerate kan redusere bufferens varighet, men koster CPU. Flysnapper
  analyserer på en fast intern rate; 96 kHz halverer derfor ikke automatisk
  pitchmotorens observasjonstid.
* Svak grunntone, støy, overlappende overtoner og en lav tone kan kreve lengre
  observasjon. For høy terskel kan også gjøre at et anslag oppdages senere.

Rapporterte målinger skiller faktisk første korrekte MIDI-beslutning fra
pluginens deklarerte 0 latency. Sistnevnte betyr at MIDI-korreksjonen ikke
legger inn en planlagt forsinkelsesbuffer; det lover ikke øyeblikkelig
audio-til-MIDI eller en bestemt fysisk rundtur gjennom brukerens anlegg.
