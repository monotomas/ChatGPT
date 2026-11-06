# Flysnapper – arbeid etter gitarprøven av 0.3.0

Brukerens prøve 6. november viser at 0.3.0 fortsatt har for mye
dobbelttrigging, utelatte toner, falske lavere oktaver og forsinkelse.
Arbeidet på `research/guitar-reliability-v4` er **ikke en ny godkjent
installasjonspakke**. En avgrenset retting finner raske monofoniske
fingerslides; den samlede polyfoniske seksstrengsprøven feiler fortsatt.
En ny prøve gjennom selve prosessoren gjenskaper også dobbelttrigging med
et originalt gitaropptak i alle åtte innstillingskombinasjonene.
Se [gjeldende status og kildebundne resultater](docs/validation/guitar-reliability-2026-11-06/README.md).

Beskrivelsen nedenfor gjelder den leverte 0.3.0 og dens funksjoner.

## Flysnapper 0.3.0

Windows x64 VST3 som gjør gitar-audio om til MIDI og korrigerer MIDI etter
skala eller et akkordspor. Sidechain og vanlig MIDI-ruting er beholdt fra
versjonen brukeren har bekreftet i Ableton.

Versjonen er beregnet på utprøving med gitar etter automatisk validering.
[Leveransenotatet](docs/RELEASE_0.3.0.md) beskriver endringene og begrensningene.
Hver Windows-pakke inneholder sine egne bygg-, test- og installasjonsbevis;
Linux-tester er ikke dokumentasjon av Windows eller fysisk Ableton-latency.

Brukerens [presisering 5. november](docs/PRODUCT_SPEC.md) er gjeldende oppdrag:
poly skal levere definerte halvtoner, monofonisk bending beholdes, og latency
skal optimaliseres og rapporteres per tone uten tidligere absolutte grenser.
Målet er en kontrollert installasjonspakke for utprøving med brukerens gitar.

## Inngang og spilleprøve

Input dynamics kombinerer gate/expander, kompressor og limiter uten lookahead.
Ni synlige kontroller kan justeres manuelt. **Learn / 20 seconds** bruker
3 sekunder med dempede strenger og normal håndkontakt, deretter 17 sekunder
representativt spill med svake/sterke anslag, akkorder og lang utklang.

En vellykket prøve stiller de faktiske kontrollene og aktiverer behandlingen.
Avbrudd eller ubrukelig prøve endrer ikke kontrollene. Manuelle endringer
avbryter pågående læring. Generert MIDI er dempet under prøven; eksternt MIDI
og akkordkontroll fortsetter. Eldre presets beholder inngangsbehandlingen av
for kompatibilitet; Learn eller manuell aktivering tar den i bruk.

[Inngangsveiledningen](docs/INPUT_DYNAMICS.md) forklarer hver kontroll og
støy/utklang-avveiningen. **Help** gir forklaringer ved musepekeren, og
**Guide** åpner veiledning inne i pluginen.

## Mono, poly og korreksjon

* Mono har valgfri bending og vibrato. Vanlig MIDI bruker en reservert
  kanal 2 for dette, med valgt +/-2 eller +/-12 halvtoner og standard RPN.
  Mottakeren må bruke samme bøyeområde.
* Poly gjenkjenner inntil seks samtidige halvtoner. Den sender Note On/Off,
  uten uavhengige bendkurver. MPE er valgfritt utgangsformat og aktiverer
  ikke polyfonisk bending.
* Genererte toner og eksternt spille-MIDI behandles av samme korreksjon.
  Fabrikkinnstillingen Fixed Scale/Chromatic gir ingen halvtoneendring.
* Chord Notes bruker de holdte akkordtonene på inngangskanal 16. Chord Scale
  bruker Buffkillers 50 akkordtyper og hver types uavhengige skalaoppsett på
  baksiden. Alle 18 opprinnelige skalaer pluss Chromatic beholdes.
* Holdte akkordnoter følger med over akkordskift. Holdte spilletoner beholder
  bindingen de startet med; nye toner bruker gjeldende akkord/skala.

## Installasjon og ruting

Windows-pakken inneholder ferdig VST3 og installer. Pakk ut hele ZIP-filen til
for eksempel en mappe på D:, lukk pluginvertene og kjør
`INSTALL_FLYSNAPPER.bat`. Velg VST3-mappen Ableton bruker. Python, CMake og
Visual Studio behøves ikke for installasjon. Ikke bruk et Linux-valideringsbygg
som Windows-plugin.

Legg Flysnapper på MIDI-/instrumentsporet og velg audio i Abletons eget
sidechain-panel. På mottakersporet velges Flysnapper-sporet og deretter selve
Flysnapper-pluginens utgang i de to MIDI From-feltene. Sett monitorering til In.
Akkordsporet sendes manuelt til Flysnapper på kanal 16; vanlig spille-MIDI
bruker kanal 1. Pluginen har stille audio-utgang; mottakerinstrumentet lager lyd.

[Brukerveiledningen](docs/BRUKERVEILEDNING.md) dekker installasjon, manuell
kopiering av hele VST3-bundlen, ruting, korreksjon og kontroller.

## Målinger og begrensninger

Mål faktisk første korrekte MIDI-beslutning, manglende og ekstra toner,
tidlige/sene Note Off og CPU-frister. Pluginens deklarerte 0 latency er ikke
den fysiske responstiden gjennom gitar, lydkort, analyse, vert og instrument.

En gate kan avvise lyd under terskelen; den kan ikke generelt skille en ønsket
tone fra like sterk støy som klinger samtidig. Svake grunntoner, sammenfallende
overtoner og sterkt inharmoniske lyder er fortsatt krevende. Lavere buffer,
ASIO og Reduced Latency When Monitoring kan redusere deler av vertens venting.
Høyere samplerate halverer ikke automatisk den interne observasjonstiden.

Tidligere forseglede tester og feil er beholdt. Ny evaluering merker poly-bend
utenfor det reviderte oppdraget og rapporterer resten uendret; den teller ikke
utelatte tilfeller som bestått. Faktisk Windows-installasjon, native VST3-testvert
og brukerens senere gitarprøve er ulike former for dokumentasjon.

## Utvikling

Behold eksisterende checkout og lokale endringer; ikke opprett et Git-worktree
uten uttrykkelig ønske. I Linux-skyen klargjør `bash Tools/cloud_setup.sh`
verktøyene; `bash Tools/cloud_validate.sh` bygger og tester med midlertidig Xvfb
og felles byggelås. Manuelle CMake/Ninja-kall må bruke samme lås, også diagnostikk.
Ikke legg en ny lås rundt skriptet som allerede holder den selv.

JUCE 8.0.16-security er låst til
`ea77444750de45e036e0279bb0291144bf8e3688`. Windows bruker Python 3.10+,
CMake og MSVC/Windows SDK ved kildebygg. Installerpakken har ingen slike krav.
`Tools/package_windows.py` krever native Windows-bygg, ferske testresultater,
originale fixture-hasher, fullkjedeprøver av dynamikk/kalibrering og reell
installasjonskontroll før en leveranse kan godkjennes.

Lisens og opphav: [LICENSE.txt](LICENSE.txt), [THIRD_PARTY.md](THIRD_PARTY.md).
