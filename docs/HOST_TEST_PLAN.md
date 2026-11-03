# Flysnapper – intern vertsverifikasjon

Dette er utviklerens kontrollplan for den integrerte pluginen. Den er ikke en
oppgave til brukeren og er ikke i seg selv et bestått Ableton-resultat.
`FlysnapperActualVst3` laster den faktiske kompilerte modulen i en JUCE-testvert;
en ekstra pluginval-kjøring kan supplere den. Driver-, Live- og fysiske
latencymålinger føres separat, med faktisk tilgjengelig vert og utstyr.

## Funksjonelle kontroller

Bruk en aktuell, bygget plugin og registrer kildecommit, binærhash, plattform,
vertsversjon, samplerate og blokkstørrelse. MPE er deaktivert i standardoppsettet.

1. Kontroller instrumentklassifisering, separat SDK kAux-sidechain og MIDI
   inn/ut på de annonserte kanalene. Velg mono eller stereo sidechain og prøv
   aktiv inngang, digital stillhet, deaktivert inngang og blokker uten samples.
2. Send spille-MIDI på kanal 1 og akkord-MIDI på kanal 16 i samme blokk, også
   på samme sampleoffset. Holdte akkordnoter skal bli med ved neste skifte;
   akkordkontroll skal aldri lekke til mottakerinstrumentet.
3. Send faktiske audio-samples gjennom den kompilerte VST3-en i både Mono og
   Poly. Kontroller genererte noter, offsets og korreksjon ved beslutningstid.
   Prøv samtidig eksternt MIDI og kollisjoner med samme genererte uttone.
4. Slå av audio-modulen og bytt Mono/Poly med holdte noter. Generert MIDI skal
   ryddes uten å avslutte eksterne noter som fortsatt holdes. Bypass, transport-
   stopp/seek og Panic skal rydde sine dokumenterte kilder og pedaler.
5. Kontroller alle korreksjonsmodi, faste skalaer, grunntransponering og
   baksidens 50 uavhengige skalaoppsett. Flip skal ikke endre MIDI eller tid.
6. Lagre og gjenåpne flere ulikt innstilte instanser. Innstillinger skal
   gjenopprettes uten gamle aktive noter og uten krysskobling mellom instanser.
7. Prøv ulike samplerater og blokkstørrelser, inkludert variable blokker,
   uvanlig mange MIDI-hendelser og gjentatt prepare/release. Registrer om
   buffere går fulle; overbelastning skal gi avgrenset, dokumentert opprydding.
8. Kontroller editorens skalering, tastatur/parameterendringer, åpning/lukking,
   forsiden og alle 50 radene på baksiden, både med og uten prosessering.

Ved fjerning av en plugin uten en siste prosesseringsblokk kan pluginen ikke
sende Note Off. Vertens opprydding på mottakersporet må derfor kontrolleres
separat i en faktisk DAW. Ingen automatisk adaptertest etablerer alle verters
støtte for MPE, uttrykkskontroll eller fjerning.

## Ableton-ruting

Den brukerdefinerte rutingen står i `BRUKERVEILEDNING.md`: audio via sidechain,
eksternt spille-MIDI på kanal 1, akkordsporets direkte utgang til kanal 16 og
pluginens MIDI-utgang som kilde for et mottakerinstrument. Kontroller denne
rutingen i Live når Live faktisk er tilgjengelig. Utviklingen skal fortsette
med de uavhengige verts- og lydtestene uten å kreve mellomtester fra brukeren.

## Lydkvalitet og forsinkelse

Den frosne grunnleggende lydkvalitetskontrollen, hele målekorpuset og separat
holdout er beskrevet i `AUDIO_VALIDATION.md`. Notegjenkjenning og fysisk
forsinkelse skal ikke godkjennes fra et åpent vindu, et signalmeter eller en
vellykket kompilering. Oppgi antall korrekte, feilaktige og manglende anslag,
første korrekte brukbare note, median, P95/P99 og maksimum. Tidlige feilanslag
rapporteres selv om en riktig note følger senere.

For fysisk ende-til-ende-måling registreres instrument, interface, driver,
samplerate, buffer, mottakerinstrument og nøyaktige start-/sluttpunkter.
Mål også MIDI-korreksjon og pitchdetektor separat. Rapporter de faktiske
fordelingene; CPU-tid og rapportert plugin-latency erstatter ikke disse målingene.
