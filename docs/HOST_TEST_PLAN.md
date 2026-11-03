# Avgrenset vertstest – Flysnapper 0.1.0 prototype

Dette er en testplan, ikke et bestått Ableton-resultat. Den automatiske testen
`FlysnapperActualVst3` laster den faktiske kompilerte VST3-modulen i en liten
JUCE-testvert. Ingen audio-driver eller DAW brukes der. Resultater fra Windows,
Live eller fysisk latency må føres separat.

## Windows / Ableton Live 12 Suite

Bruk den komplette pakken og bygg/test før installasjon. Behold Buffkiller urørt.
Oppgi Windows-versjon, eksakt Live-versjon, CPU, audiointerface/driver,
samplerate og bufferstørrelse i testresultatet. Start med MPE deaktivert.

1. Sett Flysnapper som instrument på et MIDI-spor. Velg keyboardet som vanlig
   inngang, uten å filtrere hele sporet til kanal 16.
2. Send et eget akkordspor direkte til Flysnapper-pluginen på kanal 16. Spill
   MIDI 48/52/55. Akkordvisningen skal vise C; kontrollnotene skal ikke høres på
   mottakerinstrumentet.
3. Velg Flysnapper-pluginens MIDI-utgang som MIDI From på et mottakerspor med
   et raskt instrument og Monitor In. Bruk pluginutgangen i nedre velger.
   Spill MIDI 61 på keyboardet: med Chord Notes skal MIDI 60 komme ut. Ved
   slipp skal samme ut-note avsluttes, også om akkorden er endret imens.
4. Trykk Send test note i Flysnapper og spill keyboard samtidig. Begge kilder
   skal nå mottakeren. Testnoten slippes automatisk etter 100 ms; en ekstern
   keyboardnote som fortsatt holdes skal fortsette. Den automatiserbare
   Routing Test-parameteren kan også slås av for en særskilt kildestopp-prøve.
5. Velg audio-kildesporet i Lives native sidechain-panel. Kontroller status ved
   signal, digital stillhet, No Input og sidechain av. Stillhet må ikke
   omtales som sikker frakobling. Den første prototypen utfører ikke
   pitchgjenkjenning; sidechain-lyd skal ikke høres fra Flysnapper.
6. Velg Chord Scale. Endre en rad på baksiden til Chromatic og bekreft at
   innstilling faktisk brukes. Prøv Fixed Scale og transponering. Flip skal
   ikke avslutte en holdt note eller endre MIDI-resultatet.
7. Lagre settet med flere ulikt innstilte Flysnapper-instansser. Lukk og åpne
   settet igjen. Innstillingene skal bevares uten gamle aktive noter og uten
   krysskobling mellom instansene.
8. Test play/stop, loop og seek med holdte noter, både med åpent og lukket GUI.
   Kontroller stopp/panic samt deaktivering og fjerning av pluginen. Ved
   fjerning uten en siste prosesseringsblokk kan pluginen ikke sende Note Off;
   vertens opprydding på mottakersporet må da kontrolleres særskilt.
9. Aktiver MPE som negativ kontroll. Kontroller at inkompatibel konfigurering
   varsles der den kan oppdages. Vanlig kanal-16-oppsett skal ikke hevdes å
   gi generell MPE-støtte.

Før MIDI-sampleposisjoner og faktisk mottatt note/velocity i resultatet, helst
med et MIDI-monitorverktøy. Et åpent vindu eller synlig signalnivå er ikke en
funksjonell godkjenning.

## Latencymåling

Mål først kjeden med genererte hendelser, uten pitchdetektor. Definer tydelig
hvor start og slutt er målt, og noter mottakerinstrumentets anslag. Mål deretter
pitchdetektor separat når den finnes. Pluginens rapporterte 0 samples er ingen
måling av driver-, verts-, audio-til-MIDI- eller akustisk forsinkelse.

Før antall hendelser, feil/manglende anslag, median, P95/P99 og maksimum i
millisekunder. Skille et tidlig feilanslag fra første riktige brukbare note.
Ikke godkjenn end-to-end-målene ut fra CPU-tid eller et gjennomsnitt alene.

## Resultatmal

| Område | Plattform/versjon | Resultat | Evidens/avvik |
| --- | --- | --- | --- |
| Faktisk VST3-testvert | Fra byggeloggen | Ikke kjørt i denne malen | CTest-logg |
| Windows/MSVC | Fylles etter faktisk kjøring | Ikke verifisert | Bygge-/testlogg |
| Live-ruting og gjenåpning | Fylles etter faktisk kjøring | Ikke verifisert | Kort registrering av punkt 1–9 |
| Fysisk end-to-end-latency | Interface/driver/rate/buffer | Ikke målt | Målegrense og fordelingsstatistikk |

Denne malen endrer ikke statusen til faktiske tester i leveransens testrapport.
