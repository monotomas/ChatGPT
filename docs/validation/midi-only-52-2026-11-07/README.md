# MIDI-only med 52 akkordtyper – 7. november 2026

Dette er det ferske kontrollpunktet etter brukerens to #11-tillegg.
Tidligere 50-typebevis ligger separat i `../midi-only-2026-11-07` og er
ikke brukt som om de verifiserte 52-typekoden. Ingen pitchmotor er gjeninnført.

- [Kjernen](core/result.json): 6 grupper og 2 142 807 kontroller består,
  med 10 000 målte realtime-iterasjoner uten nye allokeringer. De gamle
  50 ID-ene, suffiksene, fabrikkvalgene og Reference-headerne er uendret.
- [Samlet native-kjøring](native/full-validation.txt): alle 4 CTest-grupper
  består. GUI har 20 021 kontroller. Prosessorrapporten har 87 182 kontroller,
  12 grupper og 359 rå hendelsesrader. Den faktiske VST3-modulrapporten har
  4 292 kontroller, 21 tilfeller og 944 rader.
- [Python](python/result.json): 198 tester totalt; 196 består, 2 native
  Windows-BAT-prøver hoppes over på Linux, ingen feil. Den nye testen med
  en faktisk katalogalias dekker Windows-stifeilen fra det forrige forsøket.
- [Skjermbildene](screenshots/) er visuelt kontrollert. De siste to radene,
  52-overskriften og reset-kontrollen er synlige og lesbare. Skalanavn og
  skalatoner vises fortsatt separat i pluginen.

Linux-modulen har SHA-256
`35ab3698b22c2709e964dd91797c6a3bc9153fe6bebf258792237bd31e136707`.
Den faktisk innlastede modulen er Fx/Tools, Fsm1, med null SDK-audiobusser
og 16-kanals eventbusser inn/ut. Tester omfatter begge nye typer via kanal 16,
52 uavhengige tilordninger, import av gamle 50-typepresets, intern av/på og
sekvensiell korreksjon gjennom flere faktiske instanser.

De nye typene har nøyaktig de bestilte tonene og Lydian som foreløpig valg.
Bare kvinten kan utelates, og slik gjenkjenning merkes «no 5»; de definerende
9/#11/13/maj7-tonene utelates ikke skjult. Reelle harmoniske tvetydigheter
beholdes: eksempelvis kan det komplette Lydian-settet med kvinten i bass
også betegnes som den eksisterende major13-typen med denne basstonen som
grunntone. Testene bevarer den eksisterende bassrangeringen og viser den
nye akkorden blant alternativene; de tvinger ikke fram en oppdiktet entydighet.

`native/inputs-before.json`, `inputs-after.json` og rapportmanifestet binder
de samme 21 utvalgte prosjektinngangene. Lokal source_commit er null:
byggingen skjedde før kildeendringene ble committet. Filhashene beskriver
de faktiske kompilerte inndataene; dette er ikke full kompilator-/system-
header-/JUCE-lukning. Core-resultatet registrerte sine hashes etter kjøringen,
og skal ikke omtales som en forhåndsattestasjon. Binærfilene ble hashkontrollert,
men er ikke duplisert i Git-arkivet.

VST3-stdout har 4 293 kontroller fordi den siste kontrollen gjelder at
rapporten kunne skrives etter at rapportens tellerverdi ble tatt. Et første
skriptkall manglet kjørerettighet; `launch-permission-error.txt` er bevart.
Samme skript ble deretter kjørt via bash uten kodeendring, og dette er det
komplette beståtte løpet. Ingen kompilering eller tester ble rapportert fra
det mislykkede startkallet.

Den [faktiske Windows-kjøringen](windows/README.md) har nå bygget og installert
sin egen 52-typepakke fra e891c2e: 4/4 native-grupper og faktisk PS/BAT-
installasjon/erstatning/gjenoppretting består. Python har 197 bestått og ett
forventet symlink-hopp av 198 tester. Pakkens eksakte bytes og kilde-/modul-/
installasjonsbindinger er etterkontrollert. Tidligere mislykkede 50-type-
Windows-forsøk er bevart som separate forsøk.
Ingen faktisk Ableton Live 12.4.6-kjøring er utført her. Sporinngang,
kanalbevaring, direkte pluginruting og Device Activator kan ikke utledes fra
den eksplisitte testvertkjeden. Se det bevarte
[rutingsgrunnlaget](../midi-only-2026-11-07/host-routing/README.md).

[Den nye Excel-malen](../../factory-settings/2026-11-07-revision-2/README.md)
har 52 rader og 31 skalavalg med tonenavn i parentes. Begge nye formler og
alle ID-er er sammenholdt med Core. Fabrikkvalg venter fortsatt på brukerens
redigerte arbeidsbok. Dette kontrollpunktet er ingen ferdig brukerleveranse.
