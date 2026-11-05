# Flysnapper 0.3.0 – kalibrert inngang og gitarprøve

Denne versjonen følger brukerens presisering 5. november: poly leverer
halvtoner uten polyfonisk bending; mono beholder bending og artikulasjon.
Målet er en kontrollert Windows-installasjonspakke for utprøving med gitar.
Det er ingen påstand om feilfri transkripsjon i alle signalmiljøer.

## Endringer

* Gate/expander, kompressor og limiter med ni synlige, lagrede kontroller.
  Automatisk brumreduksjon arbeider før gaten; renset referanseinformasjon
  hindrer at kompresjon alene flytter den første MIDI-noten en halvtone.
* **Learn / 20 seconds**: 3 sekunder med dempede strenger og normal
  håndkontakt, så 17 sekunder representativt spill. Vellykket prøve setter
  faktiske kontrollverdier én gang. Avbrudd, ubrukelig prøve eller endret
  oppsett gir ingen automatisk overskriving. Etterpå kan alt justeres manuelt.
* **Help** og innebygd **Guide**, med kontrollforklaringer, ruting og råd om
  latency. Under prøven er bare audio-genererte toner dempet; akkordkontroll
  og eksternt spille-MIDI fortsetter.
* Falske lave polytoner fra spektrallekkasje er rettet i en dokumentert
  akkordovergang. En eldre monofonisk oktavfeil på G1 er rettet ved hjelp av
  periodetilpasset fysisk toneinformasjon fra allerede mottatt historie.
  Virkelige oktavskift og demping beholdes i motprøvene.
* Den eksisterende MIDI-korreksjonen, de 50 akkordtypene, alle 19 skalaer og
  brukerbekreftet sidechain-/MIDI-ruting er beholdt.

## Målt responstid

Ferske prøver gjennom hele pluginprosessoren, fra sidechain til faktisk MIDI,
gir følgende første korrekte beslutning på syntetiske toner med åtte deltoner.
Hver rad omfatter 44,1/48/96 kHz og 32/64/128 samples. Ingen absolutte
millisekundgrenser brukes som godkjenningsvilkår.

| Tone | MIDI | Median | Målt område |
| --- | ---: | ---: | ---: |
| E2 | 40 | 31,99 ms | 31,98–32,00 ms |
| A2 | 45 | 23,99 ms | 23,98–23,99 ms |
| E3 | 52 | 18,66 ms | 18,65–18,66 ms |
| A3 | 57 | 15,99 ms | 15,98–15,99 ms |
| E4 | 64 | 13,31 ms | 13,31–13,32 ms |
| A4 | 69 | 10,66 ms | 10,65–10,66 ms |
| E5 | 76 | 10,66 ms | 10,65–10,66 ms |

Dette er kausale beslutningstidspunkter, uten tilbakedatering eller etterjustert
kurveforsinkelse. Det er **ikke** målt fysisk gitar-/lydkort-/Ableton-respons.
Andre klanger og svakere anslag kan ta lengre tid eller feile. En enkel
kompilert VST3-prøve av to polytoner ga første beslutninger etter omtrent
59 og 101 ms; den er ingen generell polyfonisk latency-garanti.

Pluginens deklarerte 0 latency betyr at MIDI-korreksjonen ikke legger inn en
planlagt forsinkelsesbuffer. Den fjerner ikke observasjonstiden. Se
[inngangsveiledningen](INPUT_DYNAMICS.md) for ASIO, bufferstørrelse,
Reduced Latency When Monitoring og effekter som kan øke ventingen.

## Validering og gjenværende begrensninger

Det siste kildebundne Linux-kontrollpunktet består **15 av 15 testgrupper**,
inkludert **86 faktiske processorprøver**: kalibrering og virkelige parametre,
manuell overstyring, stillhet, utklang, slipp, mono-bending, virkelige oktaver,
MIDI-korreksjon og kompilert VST3. Alle tolv svake tone-/brumprøver har én
riktig Note On/Off; målt slipp etter fysisk kutt er 36,65–47,31 ms. Dette er
kontrollerte gitaraktige signaler, ikke menneskelige gitarframføringer.

Bredere, tidligere kjente lydsamlinger er fortsatt diagnostikk med feil:
98 av 209 reviderte konfigurasjoner består, mot 88 av 209 uten den nye
inngangsbehandlingen. Ekstra noter går fra 393 til 324 og tidlige slipp fra
54 til 51, mens sene slipp øker fra 6 til 9. Enkelte støyområder blir verre.
Dette skal ikke omskrives til full musikalsk godkjenning. Se alle utfall og
målebegrensninger i [valideringsrapporten](validation/dynamics-0.3.0/README.md).

En nivågate kan ikke generelt skille ønskede toner fra like sterke lyder som
klinger samtidig. Poly er fortsatt utsatt for overlappende overtoner, tapte
akkordtoner og ekstra stemmer. Enkelte krevende prøver gir for lange haler.
Nesten harmonisk sammenfallende kunstige kilder er fortsatt uløste tilfeller.
Den konservative falsk-basstone-rettelsen kan miste svært sparsomme signaler
som bare har femte/sjuende deltone. Ideelle kalde toner nær brumfrekvenser har
også dokumenterte, tidligere eksisterende feil. Disse er ikke skjult som
beståtte prøver.

Den separate Windows-pakkens `Validation/` inneholder native Windows-resultater,
ferske inngangsdata, DLL-/kildehashes og faktisk installer-/gjenopprettingskontroll.
Ingen Linux-fil skal installeres som Windows-plugin. Den fysiske prøven med
brukerens gitar og mottakerinstrument gjenstår, slik det reviderte oppdraget
forutsetter.
