# Bekreftet Ableton-avslag: manglende audioinngang

Den private brukerloggen viser fem ganger at Live12.4.6 laster Fsm1-
prosessoren, og deretter avviser den med «plugin has an effect category,
but no valid audio input bus». Den generelle meldingen er dermed lokalisert:
Live krever en gyldig audioinngang for kategorien Fx. DLL-en lastes før
avslaget. Kun tre komponent-/feillinjer er bevart uten stier eller andre
private loggdata. Hele råloggen er ikke kopiert til repositoriet.

Bare å legge til audioutgang fra JUCE-eksemplet vil ikke tilfredsstille den
observerte audioinngangskontrollen. Løsning med audio inn/ut kan hjelpe
innlasting som lyd-enhet, men beviser ikke den ønskede MIDI-effektrollen
før instrument eller flere instanser i samme MIDI-kjede.

Både fastlåst og siste offentlige Steinberg-header har samme44 kategorier;
ingen dokumentert MIDI-/Control-kategori ble funnet. JUCE IS_MIDI_EFFECT
velger ikke en slik VST3-kategori. Dette utelukker ikke udokumenterte
vertsfunksjoner, men det finnes ingen inspisert minimumsflaggendring som
underbygger ønsket Live-rolle.

Brukeren vurderte deretter CLAP, Max for Live og en intern prosessorkjede,
men valgte uttrykkelig å prøve den eksisterende løsningen som VST3-instrument
med audioutgang først. 0.4.1 følger dette: instrumentkategori, ingen
audioinngang og en stille stereo audioutgang. Dette er en samlet endring av
vertsrollen, ikke en påstand om at audioutgang alene reparerer Fx-kategorien.
Ingen ny audioanalyse er lagt til. MIDI hentes på et mottakerspor gjennom
pluginens MIDI-utgang, slik brukeren tidligere fikk til.

CLAP, Max og interne prosessorinstanser er utsatt. Alle 38 skalaer og 52
fabrikkvalg beholdes. Diagnosebeviset her gjelder den avviste 0.4.0-pakken;
kontroll av den nye adapteren dokumenteres separat. Den gamle publiserte
ZIP-en er merket med lastingsfeilen. Ingen ny praktisk Live-test er utført
i skymiljøet.
