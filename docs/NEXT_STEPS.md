# Gjeldende videre arbeid: MIDI-only

Brukerens [bestilling 7. november](MIDI_ONLY_REQUEST_2026-11-07.md) overstyrer
alle tidligere pitchmål. Ikke gjenoppta DSP-utvikling. Det gamle forsknings-
og målematerialet beholdes som sporbarhet.

Gjennomfør egne kontroller av MIDI-kjernen, grensesnittet, den faktiske
VST3-modulen og Windows-installasjon. Nåværende produkt skal ha null
audiobusser, 31 skalaer, 52 uavhengige kart og korrekt kanal 1/16-behandling.
Ikke svekk MIDI-testene eller presenter bortfalte audiokrav som bestått.

Før en ferdig brukerleveranse må følgende være avklart:

- Brukerens redigerte Excel-fil må valideres og eventuelle nye fabrikkvalg
  innarbeides etter stabile akkord-/skala-ID-er.
- Konkrete spor-/pluginrutingsegenskaper i Live 12.4.6 må dokumenteres:
  kanalidentitet ved Track In, valg av direkte instansadresse, bypass og
  reaktivering under en holdt akkord. Generiske VST3-tester er ikke Live.
- Hele native-/Python-/Windows-/installerkontrollen må passe til nøyaktig
  kildecommit og ny modulidentitet, også erstattings-/gjenopprettingsprøver.

Forskningsbygg kan lagres på en separat valideringsgren. Den offisielle
nedlastingsgrenen skal ikke oppdateres automatisk av et forskningsbygg.
Brukeren skal ikke få kjente utilstrekkelige mellomversjoner eller måtte
kompilere og feilsøke utviklerskript.
