# Flysnapper MIDI 0.4.1 – VST3-instrumentadapter til Live-prøve

0.4.1 endrer vertstilpasningen etter brukerens instruks: **Flysnapper
MIDI annonseres som VST3 Instrument, uten audioinngang og med stille
stereo audioutgang.** MIDI-korreksjonen, 52 akkordtyper, 38 skalaer og de
52 fabrikkvalgene fra brukerens Excel-fil beholdes.

## Hvorfor denne adapteren

Brukerens Live-logg viste at 0.4.0-prosessoren ble lastet, men at Live
avviste effektkategorien uten en valid audioinngang. Det var en konkret
innlastingsfeil i det valgte vertsoppsettet. Tidligere generiske
VST3-/Windows-kontroller etablerte ikke Live-kompatibilitet.

0.4.1 skal derfor prøves som instrument med audioutgang før videre arbeid
med en ren MIDI-effekt eller alternative formater. Audioutgangen sender
stillhet. Det tilføyes ingen synthesizer, audioinngang, sidechain,
pitchgjenkjenning eller audio-DSP.

## Bruk og identitet

Bundlen heter fortsatt `Flysnapper MIDI.vst3`, produktnavnet er
**Flysnapper MIDI**, og Fsm1-komponentidentiteten er den samme som i 0.4.0.
0.4.1 oppdaterer denne komponenten; den separate gamle 0.3.0-komponenten
overskrives ikke.

Korrigert MIDI skal hentes til et separat spor med Kontakt eller et annet
lydinstrument. Velg Flysnapper-sporet i øverste **MIDI From**, og
Flysnapper-pluginens eget MIDI-utgangspunkt i nederste felt. Mottakersporet
må monitoreres, for eksempel med **Monitor In**. Spille-MIDI bruker kanal
1, akkordkontroll kanal 16. Se [brukerveiledningen](BRUKERVEILEDNING.md).

Dette er ikke en påstand om at flere VST3-MIDI-effekter kan seriekobles
foran instrumentet på samme Live-spor. Wrapper-, Max for Live- og
CLAP-utvikling er utsatt mens brukeren prøver denne instrumentadapteren.

## Brukerens musikalske oppsett

Alle 52 tilordninger fra den utfylte Excel-filen beholdes nøyaktig: 20
endringer fra malen, inkludert fire valgte skalaer uten alle nominelle
akkordtoner. Nye instanser og **Reset all 52 to factory** bruker dette
oppsettet; lagrede instanser beholder egne innstillinger.
[Brukerfil og mapping](factory-settings/2026-11-07-user-values/README.md)
er referansen. Den kanoniske mappingens SHA-256 er
`146501dd5146f620c6718bc2be9b784380cc22eef91ec1e9dac1251c5422e814`.

Korreksjonsmodi, faste skalaer, akkordstyrte tilordninger, 38-valgs
skalamenyer, Note Off-bindinger, intern Note Correction og kanalroller
beholdes. De tidligere indeksene/ID-ene beholdes; dette lover ikke
migrering av eldre normalisert pluginautomasjon.

## Kontrollstatus

Ferske 0.4.1-bygg, native tester og Windows-/installasjonskontroller er
under arbeid. Resultater må være bundet til den nye instrumentmodulen;
[0.4.0-notatet](RELEASE_0.4.0.md) og dets bevis er historiske.

**Praktisk Ableton Live 12.4.6-aksept er fortsatt ikke bekreftet.** Det
omfatter innlasting av 0.4.1, korrekt kanal 1/16-mottak og korrigert MIDI
til mottakerinstrumentet. En generisk VST3-vert og en Windows-installasjons-
kontroll kan ikke erstatte den prøven. Pluginens null rapporterte
kompensasjonslatency er ikke et mål på hele den hørbare kjeden.
