# Hva som er kjent om ruting og bypass

Brukeren oppgir Live 12.4.6, har Max for Live og ønsker fortsatt VST3.
Brukeren opplyser at versjonen støtter seriekoblede VST3-MIDI-enheter.
Denne opplysningen er et brukeroppgitt miljøkrav, ikke en utført Live-test.

Den tidligere bevarte sekundærkopien av Live 12manualen beskriver at en
deaktivert enhet etterlater signalet ubehandlet i den vanlige signalkjeden.
`manual-evidence.json` identifiserer kilde, hash, seksjon og to korte sitater.
Kopien er fra 2024 og dokumenterer ikke detaljene i dagens 12.4.6.

Den samme kopiens 16.5.2.4 skiller eksplisitt mellom Track In som sporets
inngang og en direkte instrumentadresse som går forbi opptak/monitorering.
16.5.2.6 beskriver kanalvalg ved direkte pluginruting. 16.3 omtaler All
Channels som en sammenslått strøm, men fastslår ikke om kanalnumrene
bevares eller omsettes i den aktuelle Track In-kjeden. Dette gir grunnlag
for å skille rutene, ikke for å love at kanal 16 bevares i Live 12.4.6.

Det var for vidt å slutte fra dette til at akkord-MIDI adressert direkte
til én bestemt plugin automatisk leveres til neste plugin ved deaktivering.
Brukeren korrigerte dette. Ingen slik omdirigeringsgaranti er etablert.

Skill mellom tre tilfeller:

1. **Vanlig sporkjede:** Signal som er tilført kjeden kan gå forbi en
   deaktivert effekt. Om kanal 16 når inngangen og beholder sin identitet,
   må fastslås i den aktuelle vertsrutingen.
2. **Direkte pluginadresse:** En feed til én valgt plugin er ikke dermed
   en feed til neste plugin. Ved slik ruting er en aktiv førsteginstans
   med intern Note Correction av og Chord MIDI Through på en mulig løsning.
3. **Parallell levering:** Ruting én gang til sporet beviser ikke at verten
   sender separate kopier direkte til alle instansene. Ingen slik garanti
   er dokumentert her.

Vertens Device Activator og pluginens interne Note Correction er ulike.
Intern av/på lar chord-analysen følge mottatte hendelser hele tiden. En
vertsdeaktivert instans kan gå glipp av akkordendringer; gjenutsending av
holdte noter ved reaktivering er ikke dokumentert. At manualen ikke gir en
garanti, beviser heller ikke at verten aldri gjør det.

Hvis verten forbikobler siste instans og fører den ordinære strømmen
ubehandlet videre, kan kanal 16 nå instrumentet uten pluginens konsum.
Intern Note Correction av beholder både lytting og dette konsumet; det
er derfor den anbefalte automasjonskontrollen også ved Track In-ruting.

Runtimeimplementasjonen er rutingsagnostisk: når hendelser ankommer på 1/16,
behandles de etter den bestilte rollen. Chord MIDI Through videresender 16
til neste instans; siste instans kan konsumere 16 slik at instrumentet bare
får spille-MIDI. Et testvertsløp kan bevise denne dataveien gjennom den
faktiske modulen, men ikke Live-plassering eller den eksterne rutingen.

Det er ikke tilgang til en faktisk Live 12.4.6-kjøring i dette miljøet.
Ingen generisk testvert eller modifisert metadata skal rapporteres som en
praktisk Ableton-test. Hjelpeteksten er derfor betinget ved direkte ruting
og låser ikke alle oppsett til at første instans alltid må være på.
