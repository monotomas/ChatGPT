# Flysnapper MIDI 0.4.3 – brukerveiledning

0.4.3 er en VST3-instrumentadapter som skal prøves i Ableton Live. Den har
ingen audioinngang og sender bare stillhet på sin stereo audioutgang.
Selve funksjonen er MIDI-til-MIDI: 52 akkordtyper, 38 skalaer og brukerens
52 fabrikkvalg fra den utfylte Excel-filen. Pitch-to-MIDI og lydanalyse er
fjernet.

Korrigert MIDI hentes fra Flysnapper til et separat instrumentspor.
Denne utgaven er ikke dokumentert som en MIDI-effekt som kan seriekobles
foran et annet instrument på samme Live-spor. Se
[versjonsnotatet for 0.4.3](RELEASE_0.4.3.md) for gjeldende kontrollstatus.
Brukeren har bekreftet rutingen i 0.4.2; den beholdes i 0.4.3.

## Installasjon og pluginidentitet

Bruk Windows-pakken merket 0.4.3. Pakk ut hele ZIP-filen, lukk
pluginvertene og kjør `INSTALL_FLYSNAPPER.bat`. Python, CMake og Visual
Studio trengs ikke til normal installasjon. Ved manuell installasjon skal
hele `Flysnapper MIDI.vst3`-bundlen kopieres, ikke bare filen inne i
Contents. En Linux-modul kan ikke brukes som Windows-plugin.

0.4.3 beholder navnet **Flysnapper MIDI** og samme Fsm1-komponentidentitet
som 0.4.0. Den oppdaterer derfor denne bundlen. Gammel `Flysnapper.vst3`
0.3.0 har en annen identitet og blir ikke overskrevet. Velg Flysnapper MIDI
og kontroller versjon 0.4.3 ved utprøvingen.

Musikalsk XML-tilstand bruker stabile akkord-/skala-ID-er. Dette er ikke et
løfte om automatisk migrering av gamle prosjekter eller normalisert
19-/31-valgsautomasjon til komponentens 38 skalavalg.

Nye instanser og **Reset all 52 to factory** bruker de fire reviderte
fabrikkvalgene: C7#9 → Half Whole Diminished, C7b13 → Mixolydian b6,
C13#9 og C13add#9 → Chromatic. Lagrede tilordninger beholdes.

## Oppsettet som skal prøves i Live

1. Opprett et MIDI-spor med **Flysnapper MIDI** som instrument. Send
   spille-MIDI på kanal 1 til dette sporet. Ved direktespill bruker du
   passende monitorering, for eksempel Auto med sporet armert eller In.
   Monitor In undertrykker sporets egne MIDI-klipp; bruk Auto når du vil
   spille av klipp fra samme spor.
2. Ved akkordstyring sender akkordsporet noter til Flysnapper på kanal 16.
   Velg Flysnapper-sporet i øverste **MIDI To**, og pluginens kanal 16 i
   nederste felt. At den ønskede kanalrollen faktisk når pluginen, inngår
   i Live-prøven. Fixed Scale trenger ikke akkordsporet.
3. Opprett et separat MIDI-spor med Kontakt eller et annet lydinstrument.
   Velg Flysnapper-sporet i øverste **MIDI From**, og Flysnapper-pluginens
   eget MIDI-utgangspunkt i nederste felt. Mottakeren skal hente
   pluginens korrigerte MIDI. Sett mottakersporet til **Monitor In**, eller
   bruk Auto med sporet armert.
4. La lydinstrumentet på mottakersporet sende lyd til vanlig lydutgang.
   Flysnappers egen audioutgang er stille; det er mottakerinstrumentet som
   skal høres.

For en enkel korreksjonsprøve velger du **Fixed Scale**, grunntone C og
**Minor Pentatonic**. Med Note Correction på skal C# korrigeres til C.
Med bryteren av skal en ny C# beholde sin tonehøyde. Dette er et
kontrolloppsett for den praktiske Live-prøven, ikke et allerede utført
Live-resultat.

## Kanalroller og av/på

Kanal 1 er spille-MIDI som kan korrigeres. Kanal 16 er akkordkontroll.
Kanalene 2–15 videresendes uten tonekorreksjon. Den faktiske vertsrutingen
må bevare rollene; en testvert kan ikke bekrefte hva Live gjør før
hendelsene når Flysnapper.

La Flysnapper-enheten være aktiv under denne adapterprøven. Bruk
**Note Correction** for intern av/på: nye tonehøyder forblir uendret når
bryteren er av, mens mottatte akkordnoter fortsatt analyseres. Allerede
holdte spilletoner beholder bindingen til sin opprinnelige utgående Note
Off. FIFO-, kollisjons- og sustainreglene gjelder fortsatt.

**Chord MIDI Through** står normalt av i dette oppsettet. Da konsumeres
kanal 16 før MIDI sendes til lydinstrumentet. Bryteren beholdes for
oppsett som uttrykkelig skal videresende akkordkontroll. Den etablerer
ikke automatisk en Live-kjede eller parallell levering til flere instanser.

Vertens **Device Activator** er forskjellig fra intern Note Correction.
Vi har ikke bekreftet MIDI-utgang eller gjenutsending av holdte akkorder
ved vertsdeaktivering og reaktivering av denne instrumentadapteren.

## Korreksjonsmodi

**Chord Tones Only** korrigerer til de holdte tonene i en gjenkjent akkord. Noter som
fortsetter over et akkordskift, blir med i analysen.

**Chord-Scale Assignment** gjenkjenner akkorden og bruker denne typens
valgte skala med akkordens faktiske grunntone. Når akkorden ikke
gjenkjennes, bruker begge akkordmodusene **Chromatic** og lar nye
spilletoner beholde tonehøyden. Dette gjelder også tomt akkordinput og én
unik tone. Enkeltnoter vises som for eksempel **G (single note)**;
G B D vises som **G Major**.

**Fixed Scale** bruker valgt grunntone og skala uten å la akkordsporet
bestemme korreksjonen. Nye instanser starter i Fixed Scale/Chromatic.
Når verten uttrykkelig melder stoppet transport, brukes **Chromatic** i
alle tre moduser. Modus, grunntone og skala i kontrollene beholdes og
brukes igjen ved avspilling. Manglende transportinformasjon regnes ikke
som stopp. Allerede påbegynte toner beholder sine Note Off-bindinger.

Nærmeste tillatte tone velges. Ved lik avstand brukes de to foregående
ukorrigerte spilletonene, uten den aktuelle tonen. Vanlig legatooverlapp
beholder historikken; samtidige anslag i samme strøm bruker reservevalg
oppover. Et nytt anslag på samme korrigerte tone sender Note Off og ny
Note On på samme sample med ny velocity. Eldre Note Off-bindinger
beholdes, slik at de ikke avbryter en nyere tone som fortsatt holdes. Påbegynte toner retunes ikke ved akkord-/skala-/modusbytte.
Flysnapper venter ikke på framtidige noter.

## Baksiden og fabrikkvalgene

Baksiden har 52 uavhengige akkordtyper vist i C, inkludert CMaj13#11 og
CMaj9#11. Akkordformelen og den valgte skalaens toner vises med navn.
Skalatypen transponeres til den faktiske innkommende akkordens grunntone.
Oppsettet har fire kompakte kolonner: durgruppen (14), dominant første
del (12), dominant andre del (12) og moll med C5 og dim7 (14).
C5 står rett etter Cm. Dominantkolonnene deles mellom C7b13 og C7sus4.
Alle fire er synlige i bredden ved minste vindusstørrelse; rull loddrett
for å se resten av radene. Skalamenyen står under akkordnavnet.

Hver meny har først **Scales with all chord tones**, deretter **Chord
tones outside scale**. Alle 38 valg er tilgjengelige. Innen begge grupper
følger sorteringen parentfamilie og modusnummer. Et utropstegn og teksten
Outside scale viser at valget utelater nominelle akkordtoner.

Harmonisk dur og alle sju modusene følger etter harmonisk moll og før
diminished i menyrekkefølgen. Melodisk moll brukes stigende også ved
nedadgående spill. Enharmoniske navn kan betegne samme tonehøyde.

**Reset all 52 to factory** laster nøyaktig brukerens 52 Excel-tilordninger
i denne instansen. Nye instanser starter med samme tilordninger. De fire
valgene uten alle nominelle akkordtoner er beholdt som brukeren valgte dem.
Lagrede instanser beholder sine egne innstillinger. Kopiering kopierer
innstillingene; senere endringer er uavhengige. Se
[det endelige fabrikkoppsettet](factory-settings/2026-11-07-user-values-final/README.md).

## Hjelp og kontrollstatus

**Help** slår konteksttips av/på. **Guide** åpner den innebygde veiledningen.
**Panic** slipper sporede noter/pedaler og tømmer akkordanalysen.
MIDI-rutingstesten sender en kort C-note for å kontrollere en forbindelse.
En plugin kan ikke sende oppryddingshendelser etter at verten har fjernet den.

Pluginen rapporterer null kompensasjonslatency og bruker ikke et
analysevindu. Vertens buffere, ruting og mottakerinstrument kan likevel gi
hørbar forsinkelse. Generiske VST3-tester og Windows-installasjonskontroller
er ikke en praktisk Ableton-godkjenning. 0.4.3 skal vurderes i Live før
videre format- eller kjedeutvikling.
