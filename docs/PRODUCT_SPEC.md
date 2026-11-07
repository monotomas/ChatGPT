# Flysnapper MIDI – gjeldende produktoppdrag 0.4.3

Brukeren har valgt å prøve den eksisterende Flysnapper som **VST3
Instrument med audioutgang** før videre arbeid med en ren MIDI-effekt
eller alternative formater. Denne presiseringen går foran tidligere
påstander om null audiobusser eller samme-spor-kjeding i Live. Se
[versjonsnotatet](RELEASE_0.4.3.md) og
[brukerveiledningen](BRUKERVEILEDNING.md).

## Gjeldende leveranse

0.4.3 heter Flysnapper MIDI og beholder Fsm1-identiteten og bundlen
`Flysnapper MIDI.vst3` fra 0.4.0. Vertskategorien er Instrument, med ingen
audioinngang og én stille stereo audioutgang. Audioutgangen er en
vertsadapter; pluginen inneholder ingen synthesizer, pitchdetektor,
sidechain eller audio-DSP. 0.3.0 har separat pluginidentitet.

Alle 52 akkordtyper, 38 skalaer og eksisterende MIDI-regler beholdes.
Fire fabrikkvalg er rettet etter ny Excel-fil og siste tekstvalg: C7#9
bruker Half Whole Diminished, C7b13 bruker Mixolydian b6, og både C13#9
og C13add#9 bruker Chromatic. Ingen farge-/tekstunntak innføres. Den
bevarte [brukerfilen og mappingen](factory-settings/2026-11-07-user-values-final/README.md)
er referansen. De to tilføyde akkordene er CMaj13#11 = C D E F# G A B og
CMaj9#11 = C D E F# G B. De sju Harmonic Major-modusene inngår i de 38
skalaene. Eksisterende råindekser og stabile ID-er beholdes.

## Ruting og musikalsk atferd

Spille-MIDI mottas på kanal 1 og akkordkontroll på kanal 16. Korrigert MIDI
hentes på et separat instrumentspor: øverste MIDI From velger
Flysnapper-sporet, nederste felt velger pluginens eget MIDI-utgangspunkt.
Mottakeren monitoreres og driver Kontakt eller et annet lydinstrument.
Brukeren har bekreftet rutingen i 0.4.2. Instrumentadapteren skal ikke
beskrives som en verifisert pre-instrument MIDI-effekt på samme spor.

Chord Tones Only bruker holdte akkordtoner. Chord-Scale Assignment bruker
den gjenkjente typens egen tilordning i akkordens grunntone. Fixed Scale
bruker selvstendig grunntone/skala. Nærmeste tillatte tone og de avtalte
retningsreglene beholdes, uten å vente på framtidige noter. Påbegynte
noter beholder Note Off-bindingen ved innstillingsendringer. Ingen
gjenkjent akkord gir kromatisk gjennomslipp i begge akkordmodusene,
inkludert tomt input og én unik tone. Eksplisitt stoppet transport gir
kromatisk gjennomslipp i alle tre moduser uten å endre lagrede valg.
Ukjent transportstatus skal ikke tolkes som stopp. En enkelt tone vises
som `G (single note)`, en durtreklang som `G Major`.

Baksiden skal ha fire kompakte kolonner med 14/12/12/14 akkordtyper.
Durgruppen er først, dominant deles mellom `C7b13` og `C7sus4`, og
mollgruppen med `C5` rett etter `Cm` og `dim7` til slutt er fjerde.
Alle kolonnene skal være fullt synlige i bredden ved minste størrelse
1040 × 760; loddrett rulling er tillatt. Visningsrekkefølgen endrer ikke
typeindekser, parameter-ID-er, fabrikkverdier eller lagring.

Intern Note Correction av beholder akkordlytting og lar nye tonehøyder
være uendret. Kanal 16 konsumeres når Chord MIDI Through er av, som er
standardvalget. Kanalene 2–15 videresendes uten tonekorreksjon. Ingen
automatisk parallell akkordfeed eller vertsbestemt replay av holdte noter
forutsettes. Interne seriekoblingstester er teknisk bevis for eksplisitt
koblede moduler, ikke for plassering/kjeding i Live.

## Kontroll og videre avgrensning

0.4.0 ble avvist i brukerens Live ved valideringen av en effekt uten valid
audioinngang. 0.4.1 endret vertskategori og audioutgang etter brukerens
instruks. 0.4.2 beholder rollen og har ferske beståtte Windows-, modul- og
installasjonskontroller. Tidligere 0.4.0-resultater brukes ikke som bevis. Faktisk innlasting,
korrekt MIDI-utgang og mottakerspor i Live ble siden bekreftet av brukeren
for 0.4.2. Nye funksjoner kontrolleres separat i 0.4.3.

En ferdig Windows x64-installasjonspakke er leveransemålet. Brukeren skal
kunne installere uten Python, CMake, Visual Studio eller kildeendringer.
Generisk VST3-innlasting og Windows-/installasjonsbevis skal skilles fra
Live-resultater. Det skal ikke erklæres Live-kompatibilitet fra metadata
eller en annen testvert.

Ingen wrapper-, Max for Live- eller CLAP-utvikling er valgt nå. Slike
formatvalg og målet om en separat ren MIDI-effekt er utsatt mens denne
instrumentadapteren prøves. Pitch-to-MIDI/DSP-utvikling er avsluttet.
Historiske [0.4.0-notater](RELEASE_0.4.0.md), bestillinger og tidligere
valideringsarkiver beholdes uendret. Teksten nedenfor gjelder det
historiske gitaroppdraget og er ikke aktive krav til 0.4.1.

0.4.3 retter retningshistorikk ved legato og gir nytt anslag ved samme
korrigerte tone. Den opprinnelige to-foregående-råtoner-regelen beholdes;
samtidige anslag i samme strøm bruker reserve oppover uten lookahead.
Note Off/On ved nytt anslag beholdes sammen med opprinnelige bindinger
og siste-referanse-slipp. Routing er bekreftet av brukeren i 0.4.2.

## Historisk oppdrag 3.–6. november

Brukerens presiseringer 3.–6. november 2026 går foran begrensningene i den
opprinnelige handoveren. Utvikle hele løsningen autonomt, og gjennomfør egne
bygginger, lydprøver, regresjonstester og installasjonskontroller. Ikke krev
brukertesting av delprototyper eller en bekreftet Ableton-prøve før DSP-utvikling.

**Tilbakemelding 6. november: 0.3.0 er ikke musikalsk brukbar.** Brukeren
rapporterer hyppig dobbelttrigging, utelatte toner, falske noter i lavere
oktaver og for høy latency. Dette er produktfeil som skal undersøkes og
rettes samlet. Beståtte program-/installasjonstester eller utvalgte rene
toneprøver kan ikke brukes som ny musikalsk godkjenning. Den leverte 0.3.0
og dens opprinnelige målinger beholdes som uendret regresjonsgrunnlag.

**Funksjonsavgrensningen fra 5. november gjelder fortsatt.**
Polyfonisk bending og absolutte latency-grenser er trukket tilbake. Historiske
krav og målinger nedenfor beholdes som sporbarhet, ikke som gjeldende
leveransevilkår. Brukeren ønsker nå en gjennomarbeidet versjon til utprøving
i eget gitarmiljø etter våre automatiserte tester.

Én Windows x64 VST3 på et MIDI-/instrumentspor er hovedløsningen. Audio mottas
på ekstern sidechain. En valgfri oppdeling i to plugins er bare et teknisk
alternativ hvis den gir en konkret fordel; den skal ikke bli en ny brukeroppgave.

## To funksjonsmoduler

Pitch-to-MIDI mottar audio og leverer Note On/Off, velocity og tonehøyde.
Vanlig utgang bruker kanal 1. Uttrykksmodus kan reservere separate kanaler for
uavhengig bending. Mono/poly velges av brukeren. Modulen kan slås av uten å skade
samtidig eksternt MIDI. Ingen intern synthesizer eller dry-monitorering behøves.

MIDI-to-MIDI mottar generert og eksternt spille-MIDI samtidig. Kanal 16 er
akkordkontroll og lekker ikke til mottakerinstrumentet. Faktisk holdte noter,
inkludert noter som fortsetter over akkordskift, inngår i akkordanalysen.
Buffkillers korrigerte 50-typers bibliotek brukes for navn og numerisk tolking.

Brukeren kan velge en fast skala og grunntone, eller akkordstyrt skala fra
baksidens 50 uavhengige tilordninger. Chord Notes beholdes som direkte
akkordtonevalg fra handoveren. Alle 18 opprinnelige skalaer pluss Chromatic
beholdes, altså 19. Ingen kompatibilitetsfiltrering, skjult union eller familiearv.

Nærmeste tillatte MIDI-note velges globalt i 0–127. Ved lik avstand brukes
løpsretning og særreglene i handoveren, ellers oppover. Historikk gjelder
innnoter før korreksjon. Ingen framtidige noter ventes på. Nye noter bruker
akkorden ved sin faktiske beslutningstid; Note Off følger opprinnelig binding.
Holdt spille-note beholdes ved akkordskift. Tomt/ukjent akkordsett i akkordmodus
undertrykker nye noter. FIFO, kollisjonsreferanser og per-kilde sustain sikrer
avgrenset og dokumentert Note On/Off-adferd.

## Leveranse og bevis

Stor akkordvisning, effektivt Allowed Notes, mørkt Buffkiller-formspråk og flip
med alle 50×19 valg er del av produktet. Tilstand lagres per instans, og GUI må
ikke endre lyd-/MIDI-resultat eller beslutningstid.

En ferdig bygget Windows-installasjonspakke er ønsket leveranse. Brukeren skal
kunne pakke ut og installere uten Python, CMake, Visual Studio eller redigering
av kildekode. GitHub er godkjent for nedlasting. Ikke be om mellomliggende
produktprøver; løs utviklings- og pakkingsproblemer selv.

Målene om ekstremt lav audio-til-MIDI-latency står ved lag. Mål første korrekte
brukbare note, falske anslag, manglende toner og CPU-frister både på syntetiske
signaler og faktiske gitaropptak. E2<10 ms og E3<5 ms samlet kjede er brukerens
mål, ikke en egenskap som kan erklæres fra kompileringssuksess. Dokumenter
målte avvik i den endelige leveransen; ikke skjul lengre analysevinduer eller
fjern polyfoni som en umerket produktendring.

## Bekreftet ruting og tidligere leveranse

Brukeren har bekreftet at sidechain-audio, MIDI-utgangen og all ruting fungerer
i Ableton. Behold den samlede instrument-pluginen og vertens eksisterende
ruting. Dette er brukerens observasjon; det er ikke en automatisert Live-test
utført i skymiljøet.

Leveransen 0.2.1 rettet seg mot gitarens dynamikk og klangendringer: raskere
ansats der observasjonen er entydig, videre tonefølging gjennom avklinging,
færre falske oktavskifter i begge retninger og ekte monofonisk pitch-bend.
Genuint nye toner, virkelige oktavskifter, demping, stillhet og støy må fortsatt
behandles korrekt. En lengre vilkårlig holdetid er ikke en løsning på for
tidlige Note Off-er. De opprinnelige kvalitetssjekkene beholdes.

I 0.2.1 følger valgfri Mono Bend kontinuerlig tonehøyde og vibrato rundt den korrigerte
ankernoten. Den bruker vanlig MIDI pitch wheel på en reservert kanal 2, slik at
eksternt spille-MIDI på kanal 1 beholder sine noter og uttrykk. MPE er ikke et
krav. Valgbart bendområde er +/-2 eller +/-12 halvtoner, med standard MIDI RPN
for følsomheten; mottakere som ignorerer RPN må bruke samme innstilling manuelt.
Kanal og område vises i grensesnittet. Når Mono Bend er av, og i Poly, brukes
fortsatt den eksisterende noteutgangen på kanal 1. Poly har ikke separate
bendkontrollere per stemme i denne oppdateringen.

Kanaladskillelsen gjelder Flysnappers egen MIDI-strøm. Verten kan slå kanaler
sammen, og et mottakerinstrument kan bruke pitch wheel globalt for én lyd.
Uavhengig uttrykk for samtidig keyboard og bøyd audio kan derfor ikke loves
for en slik mottaker. Dokumenter et eget mottakerspor når denne adskillelsen
trengs; ikke krev MPE for vanlig monofonisk audio-bend.

## Gjeldende krav etter tilbakemelding 4. november

0.2.1 er ikke akseptert som musikalsk ferdig. Brukeren ønsker ikke flere
mellomversjoner, manuell feilsøking eller prøving av kjente utilstrekkelige
installasjonspakker. Utvikling og undersøkelser skal fortsette autonomt.
Hvis nødvendige egenskaper fremdeles mangler, skal de stå som uløste; en
kompilering eller gode utvalgte syntetiske prøver er ikke en ferdig leveranse.

Normal tonegjenkjenning må fungere på et moderat gitarsignal. Den skal ikke
forutsette at brukeren forsterker både signal og støygulv kraftig. Innebygd
behandling må avvise brum og ikke-tonale kontakt-, dempe- og knirkelyder,
samtidig som svake anslag og ekte avklingende strenger beholdes. Målinger må
skille støy fra svake ønskede toner; en absolutt volumgrense eller lang
vilkårlig holdetid oppfyller ikke dette kravet. Akustisk identiske signaler kan
ikke avsløre utøverens hensikt, og denne begrensningen skal ikke skjules.

Med bending aktivert skal ekte jevne bends og vibrato gi kontinuerlig
tonehøyde. Fingerslides over bånd, hammer-ons og pull-offs skal fortsatt gi
nye noter når strengen faktisk etablerer nye diskrete toner. Manglende tydelig
volumanslag er ikke tilstrekkelig grunn til å velge bend. Raske jevne bends
som lander på en eksakt halvtone skal heller ikke gi falske nye anslag.
Klassifiseringen må undersøke tidsforløp og uavhengige akustiske tegn, og
testene må inneholde begge forklaringene til tilsynelatende like pitchskift.

Polyfonisk modus skal støtte flere samtidige ringende strenger og uavhengig
bending per stemme. Én global pitch wheel kan ikke oppfylle det siste kravet.
Standard MIDI MPE med separate medlemskanaler er et tillatt uttrykksformat;
vanlig MIDI og den eksisterende monofoniske utgangen beholdes. Kanal 16
forblir akkordkontroll og skal ikke brukes til utgående spilletoner.
MPE-vert og mottaker må tolke formatet riktig. At en egen mottakertest gjør
det, er ikke i seg selv dokumentasjon av Abletons atferd.

Forsinkelse, feilanslag, falske oktaver, manglende toner, tidlige Note Off-er,
bendkurver og CPU-frister skal vurderes samlet. Lavere forsinkelse oppnådd
ved å slippe gjennom ekstra eller feil toner teller ikke som forbedret
musikalsk brukbarhet. Se [målepolicyen](../Tests/AudioFixtures/readiness/policy.json)
og [arbeidsstatusen](MUSICAL_READINESS.md). Policyens forskningsgrenser var
mellomliggende målepunkter under bestillingen 4. november.

## Gjeldende krav etter presisering 5. november

Denne delen erstatter motstridende krav om polyfonisk bending, de absolutte
E2/E3-latency-grensene og kravet om å bevise full musikalsk ferdigstillelse før
en første prøve på brukerens gitar. Ingen gammel målepolicy eller måleresultat
skal skrives om som følge av den endrede bestillingen.

* Poly skal gjenkjenne flere samtidige, definerte halvtoner og sende vanlige
  Note On/Off til MIDI-to-MIDI-delen. Uavhengig polyfonisk bending er ikke et
  krav og skal ikke aktiveres av MPE-valget. Monofonisk bending og skillet
  mellom bends, fingerslides, hammer-ons og pull-offs beholdes.
* En kausal dynamikkprosessor foran pitchmotoren kombinerer gate/expander,
  kompressor og limiter. Åpne-/lukkegrense, attack, hold/release, kompresjon,
  inngangstrim og limitergrense har synlige, lagrede brukerinnstillinger.
  Den tilfører ingen lookahead-blokk. Støy som overlapper en ønsket tone,
  eller er sterkere enn den, kan ikke generelt skilles ut av en nivågate.
* En veiledet prøve varer omtrent 20 sekunder: først 3 sekunder stille med
  tilkoblet gitar, deretter 17 sekunder representativt spill, inkludert svake
  anslag og utklang. Analyse skjer på det faktiske sidechain-signalet.
  Vellykket fullført prøve stiller de synlige kontrollene som utgangspunkt;
  brukeren kan overstyre etterpå. Avbrutt eller ubrukelig prøve endrer ikke
  innstillingene. Manuell endring under prøven avbryter den. Genererte toner
  dempes under prøven; eksternt spille-MIDI og akkordkontroll fortsetter.
* Hjelpetekster forklarer kontrollene, støy/utklang-avveiningen, kalibreringen,
  mono/poly, ruting og praktiske tiltak for lavere forsinkelse i verten.
  Gjeldende akkord-/skalafunksjoner og brukerbekreftet Ableton-ruting bevares.
* Latency optimaliseres og rapporteres per tonehøyde med første korrekte
  beslutning, manglende/ekstra toner, tidlig/sen Note Off og CPU-frister.
  Ingen absolutt millisekundgrense brukes som nytt godkjenningskrav.
  Pluginens rapporterte 0 latency er ikke fysisk audio-til-MIDI-responstid.
* Målet for 0.3.0 er en ferdig bygget, kontrollert Windows-installasjonspakke
  som kan prøves med brukerens gitar. Kalibrering, faktisk processor-/VST3-
  kjede og installasjon må testes, og kjente begrensninger rapporteres.
  Det er ikke en påstand om feilfri polyfonisk transkripsjon av alle signaler.

Den opprinnelige 30-tilfellers regresjonsporten beholdes. Ny evaluering merker
tydelig hvilke tidligere poly-bend-/stemmeidentitetskrav som er utenfor scope,
mens faktiske manglende akkordtoner, falske overtonestemmer og tidlige Note Off
fortsatt måles. Nesten harmonisk sammenfallende, kunstige kilder beholdes som
diagnostikk dersom de ikke kan skilles sikkert innen et kort kausalt vindu;
de skal ikke skjules i en aggregert beståttprosent eller kalles løst.
