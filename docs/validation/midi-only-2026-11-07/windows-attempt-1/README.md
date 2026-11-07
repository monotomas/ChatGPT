# Første Windows-CI-forsøk: stopp før bygging

Kildecommit `98b98f08ee5d561c013357784193e3be28319699`,
[GitHub-kjøring 42380307622](https://github.com/monotomas/ChatGPT/actions/runs/42380307622),
feilet etter 22 sekunder. De offentlige kjøre-/jobbsidene viste feil i de
to publiseringsstegene; native-bygg og Python-regresjoner startet ikke.
Ingen installasjonspakke eller statusgren ble opprettet.

Detaljerte Windows-logger krevde innlogging og ble ikke hentet. Den
lokale reproduksjonen kjører faktisk PowerShell og viser den matchende
argumentfeilen: et enkelt strengresultat ble splattet som tegn. En
typet strengtabell bevarer `--research` som ett argument. Dette er
utført reproduksjon, ikke et påstått utdrag fra utilgjengelig Windows-stderr.

JSON-en binder reproduksjonens tre bevarte filer. Hashene for de
offentlige HTML-sidene beskriver lokale observasjoner; hele HTML-filene
er ikke duplisert i dette arkivet. Den nye regresjonen kjører begge
produksjonsuttrykkene med både forsknings- og releasegrenen.

Endringen gjelder arbeidsflyten, uten endring av de 21 utvalgte native
prosjektinngangene. Den neste Windows-kjøringen må fortsatt bygge og
verifisere sin egen faktiske pakke. Ingen Ableton-test er utført her.
