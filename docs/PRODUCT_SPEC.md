# Flysnapper – gjeldende produktoppdrag

Brukerens presiseringer 3.–4. november 2026 går foran begrensningene i den
opprinnelige handoveren. Utvikle hele løsningen autonomt, og gjennomfør egne
bygginger, lydprøver, regresjonstester og installasjonskontroller. Ikke krev
brukertesting av delprototyper eller en bekreftet Ableton-prøve før DSP-utvikling.

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
og [arbeidsstatusen](MUSICAL_READINESS.md). Policyens forskningsgrenser er
mellomliggende målepunkter; de erstatter ikke de opprinnelige latency-målene.
