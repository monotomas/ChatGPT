# 38 skalaer / 52 akkordtyper – harmonisk dur

Dette er fersk verifikasjon etter brukerens harmonisk-durtillegg.
Tidligere 31-skalaresultater er separate arkiver, ikke attestasjon av denne
koden. Harmonisk moll og alle tidligere skalasett er beholdt.

Harmonic Major 1–7 er lagt til som stabile indekser 31–37, med nøyaktige
navn fra bestillingen og moduser av parentsettet 0, 2, 4, 5, 7, 8, 11.
Visningsfamilien følger harmonisk moll og kommer før diminished. De gamle
31 indeksene, ID-ene, navnene, tonemaskene og alle 52 fabrikkvalg er bevart.

- [Core](core/result.json): 6 grupper, 2 191 245 kontroller; 10 000 målte
  realtime-iterasjoner uten nye allokeringer.
- [Native](native/full-validation.txt): 4/4 CTest-grupper består. GUI har
  20 135 kontroller, prosessor 107 320 / 14 grupper / 2 377 rå hendelser,
  faktisk VST3-modul 4 964 rapporterte kontroller / 21 tilfeller / 1 112
  hendelser. Stdout har én ekstra kontroll av siste rapportskriving.
- [Python](python/result.json): 198 totalt; 196 består, 2 native Windows-
  BAT-prøver hoppes over på Linux, ingen feil.
- [Regnearkkontrollen](worksheet-check.json): alle 38 ID-er/navn/tonemasker
  og familie-/modusrekkefølge stemmer med Core. Alle 52 raders ID-er/navn/
  forhåndsvalg stemmer; nye noteangivelser er sammenholdt med MIDI-intervallene.

Linux-modulens SHA-256 er
`1ed8aac9b162652123e79be86b8896fd6007132e203b118a5850200c2bc6c679`.
Null SDK-audiobusser, MIDI inn/ut og samme Fsm1-komponentidentitet er
kontrollert i den faktisk innlastede modulen. Ingen lydmotor er gjeninnført.

Utvidelsen krevde to nødvendige tilpasninger. Snapshot-protokollens fem
skalabiter var utilstrekkelige for indeks 31–37. Ubrukt bit 58 bærer nå den
sjette biten; gamle statusord og øvrige felt er uendret. Testene dekker
3 042 kombinasjoner av skala, grunntone, modus og korreksjonsstatus.

Med 38 valg gir float-normalisering av enkelte ferske standardverdier
f.eks. 2.999999762 i stedet for 3. Direkte int-konvertering kunne da velge
feil skala ved første callback og ved eksport av stabile ID-er. Diskrete
prosessorlesinger avrundes nå før de begrenses. En reell fersk konstruktør-
prøve, uten setter eller innsprøytet råcache, krever Harmonic Minor (intern indeks 3)
og Bb→B i det aktuelle fabrikkoppsettet. UI-avrunding fantes allerede;
aktive og stillestående forhåndsvisninger av de nye valgene er kontrollert.

Råparametercache og faktisk diskret AudioParameterChoice-indeks er ulike
begreper. Tester som gjelder logiske fabrikkvalg leser valgets indeks;
den separate runtime-regresjonen må fortsatt avsløre feil ved råcachelesing.
Hvis brukerens Excel-fil endrer den nåværende fabrikkraden med indeks 3,
skal testen tilpasses brukerdata uten å tvinge fabrikkvalget tilbake.

21 utvalgte prosjektinnganger er identiske før/etter native-kjøringen.
Lokal source_commit er null fordi kilden ble bygget før commit; eksplisitte
filhashes identifiserer inngangene. Dette er ikke full kompilator-/system-
header-/JUCE-lukning. Core-resultatets hashes er registrert etter kjøringen.
Binærfiler er hashkontrollert og utelatt fra Git-arkivet.

Windows-pakken skal ha sin egen ferske 38/52-verifikasjon. Ingen faktisk
Ableton Live-kjøring er utført her; testvertens eksplisitte kjede beviser
ikke Lives plassering, kanalbevaring eller Device Activator-ruting.
Fabrikkvalg avventer fortsatt brukerens redigerte Excel-fil.
