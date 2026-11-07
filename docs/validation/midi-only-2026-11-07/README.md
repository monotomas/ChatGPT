# MIDI-only-kontrollpunkt 7. november 2026

Dette er teknisk kontroll av den nye Flysnapper MIDI0.4.0. Brukeren har
avsluttet Pitch-to-MIDI-oppdraget. Gamle mislykkede audioprøver er historikk,
ikke tester som nå kalles bestått ved å fjerne modulen.

Gjeldende runtime har31 skalaer,50 akkordtyper, MIDI inn/ut og ingen
audiobusser. Den har egen komponentidentitet og erstatter ikke den gamle
Flysnapper0.3.0-instrumentkomponenten. Fabrikkvalgene er foreløpig de samme;
ny brukerbestemt data venter på den redigerte Excel-filen.

## Kjørte kontroller

- [Kjernen](core/README.md):2,140,697 assertions, inkludert uavhengige
  modalformler, eksakte bebopsett, alle50 menyers31 valg og gamle
  MIDI-regresjoner. Ingen nye allokeringer i10,000 målte kjerneiterasjoner.
- [Samlet native-bygg](native/README.md): alle4 aktuelle CTest-grupper
  består. Den faktiske prosessoren har82,633 kontroller i11 grupper;
  den innlastede VST3-modulen har4,084 rapporterte kontroller i20 tilfeller.
- [Første Python-/pakkekontroll](python/unittest.txt): 196 tester totalt,
  194 består og 2 native BAT-prøver hoppes over på Linux. Portable
  PowerShell-transaksjoner, komponentisolasjon og pakke-/bevisavvisning er med.
- [Ny Python-kontroll](windows-attempt-1/python-regressions.json): 197 tester
  totalt, 195 består og de samme 2 BAT-prøvene hoppes over. Den inkluderer
  den faktisk kjørte PowerShell-regresjonen for CI-argumentfeilen.
- [Første Windows-CI-forsøk](windows-attempt-1/README.md) stoppet i
  publiseringssteget før bygging; dette er ikke et Windows-byggresultat.
- [Uavhengig byte-/datarevisjon](independent-audit-binding.json) bekrefter
  kildeinput, rapportbindinger og uendrede Excel-fabrikkvalg.
- [Skjermbilder](screenshots/) viser den aktuelle MIDI-only-forsiden,
  baksiden med siste rad og den innebygde veiledningen.

Det kompilerte Linux-modulhashset er
`81831a3729e43e82d38d947b6ab2c0b1360fb5d382fb460d07e1c00a8a0b4714`.
Den viser Fx/Tools, egen Fsm1-identitet, null SDK-audiobusser og16-kanals
MIDI-eventbusser inn/ut. Rapportene binder21 utvalgte prosjektinnganger
og native-testprogrammene til kjøringen; det er ikke full
compiler-/systemheader-/JUCE-byggelukning.

Lokal `source_commit` er null i byggmanifestet. Filhashene binder den
arbeidskopien som faktisk ble bygget; dette er ikke en påstand om et
Git-commitstempel før koden ble lagret. Windows-CI skal registrere sitt
faktiske kildecommit separat. Native binærfiler er hashregistrert, ikke
duplisert i dette arkivet.

## Rettede feil i kontrollrunden

Første byggforsøk kompilerte, men inngangsvernet oppdaget at GUI-kilder
hadde endret seg under kjøringen. Det ble ikke brukt som fersk attestasjon.
Neste forsøk fant en reell GUI-cachefeil ved stillestående vert; lesing av
parameterens faktiske verdi løste den uten å svekke testen.

Skalaprøvens forventning for harmonisk-mollmodus4 var feilskrevet. En
uavhengig rotasjon av parentsettet og de bevarte modulutdataene viser at
modulen allerede valgte riktig Dorian♯4. Forventningen ble rettet;
det ble ikke gjort en unødvendig processorendring. En separat normalisert
parameterkontroll over31 skalaer×12 grunntoner bekrefter valgkonverteringen.

Et GUI-bilde ble tatt før50ms-visningstimeren hadde oppdatert skalateksten.
Den siste testen lar ekte GUI-timere oppdatere og krever C/Ionian, korrekt
tonemaske og Db→D før bildet tas. Det nåværende bildet er kontrollert visuelt.
Valgfri vindusstørrelseslagring ble fjernet for å unngå usynkronisert
ValueTree-tilgang ved vertens tilstandslagring; vanlig vindusendring beholdes.

## Det disse resultatene ikke beviser

Ingen av disse kjøringene foregår i Ableton Live12.4.6. Ruting til sporets
inngang, direkte pluginadresser, parallell levering og reaktivering av
en deaktivert enhet må ikke utledes fra den eksplisitt koblede testkjeden.
Se [rutingsgrunnlaget](host-routing/README.md).

Det er ikke en ferdig brukerleveranse før fabrikkvalg og de nødvendige
verts-/Windows-kontrollene er avklart. Et forskningsbygg kan lagres på
`validation/midi-only-ci`; den offisielle nedlastingsgrenen skal ikke
automatisk få dette som en godkjent ny versjon.
