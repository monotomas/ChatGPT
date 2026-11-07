# Flysnapper MIDI 0.4.0 – brukerveiledning for utviklingskandidaten

Dette dokumentet gjelder MIDI-only-utgaven. Den tekniske Windows-/verts-
verifikasjonen må være ferdig før en installasjonspakke kalles godkjent.
Praktisk Live 12.4.6-ruting er ikke verifisert i dette miljøet.

## Installasjon og eldre versjon

En eventuell Windows-pakke inneholder ferdig VST3 og installer. Pakk ut hele
ZIP-filen, lukk pluginvertene og kjør `INSTALL_FLYSNAPPER.bat`. Python, CMake
og Visual Studio er utviklerverktøy og trengs ikke til normal installasjon.
En Linux-modul kan ikke brukes som Windows-plugin.

Den nye bundlen heter `Flysnapper MIDI.vst3`. Kopier hele bundlen hvis
manuell installasjon brukes, ikke bare filen inne i Contents. Den nye
komponenten har egen identitet; gammel `Flysnapper.vst3` 0.3.0 blir ikke
overskrevet. Velg **Flysnapper MIDI** når du legger inn den nye utgaven.
Gamle prosjekter og normalisert 19-valgsautomasjon er ikke automatisk
migrert til den nye 31-valgskomponenten.

## Kanalroller og flere instanser

Spille-MIDI ankommer på kanal 1. Akkordsporet sender kontrollnoter på kanal 16.
Kontakt eller et annet mottakerinstrument kan ligge på et separat spor.
Flysnapper korrigerer noter uten å vente på framtidige noter.

Ved seriekobling bruker neste aktive instans spille-MIDI som allerede er
korrigert av den forrige. Aktiver **Chord MIDI Through** i hver instans
som skal sende akkordkontroll til en følgende Flysnapper. Siste instans
skal konsumere 16, slik at mottakerinstrumentet ikke får akkordnotene som
spilletoner. Det trengs ikke en egen akkordkanal per instans.

**Note Correction** er den interne korreksjonsbryteren. Når den er av,
forblir nye tonehøyder uendret, men mottatte akkordnoter analyseres fortsatt.
Allerede holdte spilletoner beholder sin opprinnelige utbinding til Note Off.
De eksisterende FIFO-, kollisjons- og sustainreglene beholdes; dette er ikke
et løfte om byte-identisk videresending av alle pedal-/noteforløp.

Vertens **Device Activator** er noe annet. En deaktivert instans kan gå
helt ut av prosessering. Signal i den ordinære kjeden kan gå forbi den,
men en separat feed direkte adressert til denne instansen blir ikke dermed
omdirigert til neste. Ved slik direkte ruting må første mottaker forbli
aktiv i verten; automatér intern Note Correction i stedet.

Brukeren ønsker primært kanal 16 til selve sporets inngang. Om denne
rutingen tilbyr og bevarer kanal 1/16 gjennom den aktuelle Live-kjeden,
må bekreftes der. Det samme gjelder hvilke instanser vertsvelgeren
adresserer, og hva som skjer med holdte akkordnoter ved reaktivering.
Ingen automatisk parallell feed til alle instanser er forutsatt av koden.

Når MIDI faktisk går inn i den vanlige sporkjeden med kanalene bevart,
trenger ikke første instans å være aktiv bare for å videresende nye
hendelser: vertens vanlige bypass kan føre dem videre. En senere aktivert
instans kan likevel mangle en akkord som startet mens den var deaktivert.
Hvis siste instans også forbikobles av verten, kan kanal 16 nå instrumentet
uten å bli konsumert. Automatér derfor helst Note Correction og behold
akkordlytting/kanalhåndtering i drift.

## Korreksjonsmodi

**Chord Tones Only** korrigerer til de holdte akkordtonene. Noter som
fortsetter over et akkordskift, blir med i analysen.

**Chord-Scale Assignment** gjenkjenner akkorden og bruker denne typens
valgte skala med akkordens faktiske grunntone. Tomt eller ukjent
akkordgrunnlag skaper ikke en oppdiktet skala.

**Fixed Scale** bruker valgt grunntone og skala uten å la akkordsporet
bestemme korreksjonen. Fabrikkoppsettet er Fixed Scale/Chromatic.
Nærmeste tillatte tone velges med de avtalte retningsreglene ved lik avstand.
Påbegynte toner retunes ikke ved et senere akkord-/skala-/modusbytte.

## Baksiden og skalaene

Baksiden har 52 uavhengige akkordtyper vist i C. Akkordformelen og den valgte
skalaens toner vises med navn. Den valgte skalatypen transponeres til den
faktiske innkommende akkordens grunntone.

Hver meny har først **Scales with all chord tones**, deretter **Chord tones
outside scale**. Alle 31 valg beholdes. Innen begge deler følger sorteringen
parentfamilie og modusnummer. Et utropstegn og teksten Outside scale viser
at valget utelater navngitte akkordtoner. Det legges ikke til skjulte toner
for å gjøre valget kompatibelt.

Melodisk moll brukes stigende også ved nedadgående spill. Tonene i
akkordtypeformlene inkluderer nominelle toner som gjenkjenningen kan
akseptere utelatt. Enharmoniske navn kan betegne samme tonehøyde.

**Reset all 52 to factory** gjelder bare denne instansen. Nye fabrikkvalg
skal komme fra brukerens utfylte Excel-fil; ved dette punktet er de
opprinnelige 50 tilordningene beholdt, med foreløpig Lydian på de to nye
#11-typene. Kopiering av instanser kopierer deres
musikalske innstillinger, og senere endringer er uavhengige.

## Hjelp og opprydding

**Help** slår konteksttips av/på. **Guide** åpner den innebygde veiledningen.
**Panic** slipper sporede noter/pedaler og tømmer akkordanalysen.
MIDI-rutingstesten sender en kort C-note for å kontrollere en forbindelse.
Fjernelse av en plugin uten en siste prosesseringsblokk kan kreve vertens
opprydding; pluginen kan ikke sende data etter at den er borte.

MIDI-prosessoren legger ikke inn et analysevindu eller tilsiktet venting.
Vertens buffere/ruting og mottakerinstrumentet kan likevel bidra til
hørbar forsinkelse. Ingen fysisk 0 ms- eller Ableton-godkjenning følger av
at modulen rapporterer null kompensasjonslatency.
