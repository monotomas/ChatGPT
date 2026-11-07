# Samlet Linux-native MIDI-kontroll

`full-validation.txt` er den siste komplette kjøringen av det låste
valideringsskriptet. Alle4 aktuelle grupper består: Core, PluginSmoke,
MidiOnlyProcessor og ActualVst3. Byggingen holder den delte native-låsen
og starter/stopper midlertidig Xvfb. Ingen installasjon utføres.

Prosessorrapporten har82,633 kontroller,11 grupper og347 rå hendelsesrader.
VST3-rapporten har4,084 kontroller,20 tilfeller og936 rader. VST3-stdout
viser4,085 fordi den siste kontrollen av at rapporten kan skrives, utføres
etter at rapportens kontrolltall er tatt. Dette er samme kjøring, ikke to
ulike resultatsett.

Kontrollene laster den faktiske modulen og sjekker SDK-busser/metadata,
31 skalaer, seriekoblede instanser, alle interne korreksjonskombinasjoner,
kanal16-videreføring, kopi av tilstand, gamle notebindinger og MIDI-offsets.
Kaskaden65→66→67 skiller sekvensiell behandling fra bare siste trinn,
som ville gitt65→64. Intern korreksjon av under en holdt akkord testes
med en senere aktivering uten at akkorden sendes på nytt.

Dette beviser den eksplisitte testvertsdataveien når hendelsene leveres
på riktige kanaler. Det beviser ikke Lives eksterne ruting eller hvordan
Device Activator behandler en direkte pluginfeed. En test av et kalt
`processBlockBypassed` er ikke bevis for at Live alltid kaller det.

`midi-validation/report-manifest.json` binder21 utvalgte filhashes,
rå rapporter, hendelser, modulen og testeksekverbarene. Den lokale
Git-commitverdien er null; filhashene er den bevarte kildeidentiteten.
Det er ikke et komplett eksternt kompilator-/headeravhengighetsbevis.
`excluded-binaries.json` registrerer de binærfilene som ikke er kopiert.

Gamle forsøk og en etterfølgende bevaring av forsøket3s21 inngangsfiler
finnes lokalt under `build/midi-only-native-attempt*`. De må ikke blandes
med dette endelige løpet. Bildet fra forsøk3 viste en ikke oppdatert
skalatekst og skal ikke brukes som den nåværende forsiden.
