# Gjeldende videre arbeid: MIDI-only

Brukerens [bestilling 7. november](MIDI_ONLY_REQUEST_2026-11-07.md) overstyrer
alle tidligere pitchmål. Ikke gjenoppta DSP-utvikling. Det gamle forsknings-
og målematerialet beholdes som sporbarhet.

MIDI-kjernen, grensesnittet, den faktiske VST3-modulen og Windows-installasjon
har ferske beståtte kontroller for 52-typekoden. Nåværende produkt har null
audiobusser, 31 skalaer, 52 uavhengige kart og korrekt kanal 1/16-behandling
i testverten.
Ikke svekk MIDI-testene eller presenter bortfalte audiokrav som bestått.

Før en ferdig brukerleveranse må følgende være avklart:

- Brukerens redigerte Excel-fil må valideres og eventuelle nye fabrikkvalg
  innarbeides etter stabile akkord-/skala-ID-er.
- Konkrete spor-/pluginrutingsegenskaper i Live 12.4.6 må dokumenteres:
  kanalidentitet ved Track In, valg av direkte instansadresse, bypass og
  reaktivering under en holdt akkord. Generiske VST3-tester er ikke Live.
- Hele native-/Python-/Windows-/installerkontrollen må passe til nøyaktig
  kildecommit og ny modulidentitet, også erstattings-/gjenopprettingsprøver.
  Dette er gjort for e891c2e. Nye fabrikkendringer må få tilsvarende egen
  kilde-/pakkebinding før endelig levering; ikke omdøp den foreløpige pakken.

Forskningsbygg kan lagres på en separat valideringsgren. Den offisielle
nedlastingsgrenen skal ikke oppdateres automatisk av et forskningsbygg.
Brukeren skal ikke få kjente utilstrekkelige mellomversjoner eller måtte
kompilere og feilsøke utviklerskript.
