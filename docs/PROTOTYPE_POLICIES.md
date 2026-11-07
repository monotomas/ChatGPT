# Historiske note- og overgangsregler

> Historisk dokument fra det tidligere kombinerte audio-/MIDI-oppdraget.
> Gjeldende MIDI-regler står i [Core-veiledningen](../Source/Core/README.md),
> [produktoppdraget](PRODUCT_SPEC.md) og [0.4.3-notatet](RELEASE_0.4.3.md).
> Fra 0.4.3 beholdes historikk ved vanlig legatooverlapp, og nye spilleanslag
> reartikulerer samme ut-tone. De eldre sammenslåings- og overlappsreglene
> nedenfor, samt gamle antall/lydmodulbeskrivelser, er ikke aktive krav.

Denne filens historiske navn er beholdt for eldre lenker. Gjeldende regler er
implementeringsvalgene for den samlede Flysnapper, etter brukerens presisering
og godkjenning 3. november 2026. `PRODUCT_SPEC.md` går foran eldre stoppkrav i
handoveren. Utvikling og tester utføres autonomt; det kreves ingen delprototype-
eller rutingsgodkjenning fra brukeren for å arbeide videre.

## Fastlagte regler som er implementert

- Tre moduser i MIDI-to-MIDI: Chord Notes, Chord Scale og Fixed Scale. Korreksjon kan omgås.
- Korreksjon finner nærmeste tillatte MIDI-note innen 0–127, på tvers av oktavgrenser. Mottatte akkorders register begrenser ikke utøverens register.
- Chord Notes bruker bare de faktisk holdte pitch classes. Duplikater og oktavdoblinger teller hver for seg ved Note Off.
- Chord Scale bruker gjenkjent type og grunntone til å slå opp brukerens valgte skala. Den forener ikke skalaen med akkordtonene i skjul. Alle 50 typer har egne, stabile ID-er og alle 19 skalavalg.
- Fixed Scale bruker manuelt valgt grunntone og skala. Chromatic tillater alle pitch classes.
- Note Off følger bindingen opprettet av Note On, selv når akkord eller oppsett er endret. Velocity-zero Note On er Note Off. Vanlig ekstern notevelocity beholdes ved en ny ut-note.
- Samtidige kontrollhendelser på samme sample behandles først; deretter dannes nye utøvernoter mot dette settet. Hendelser på senere sample i samme blokk påvirker ikke tidligere noter.
- Detekterte noter og eksternt MIDI bruker samme korreksjon. De har separate tekniske kilde-ID-er for korrekt avslutning; dette er ikke egne funksjonsmoduler. Ingen ekstra blokk eller framtidig note ventes på.
- GUI, flip og lukket editor styrer ikke notenes tid eller levetid. Allowed Notes kommer fra det aktive numeriske prosesseringssettet.

## Overganger og noteidentitet

| Situasjon | Uttrykkelig regel | Musikalsk konsekvens |
|---|---|---|
| Akkord, skalatype, grunntone eller skalatabell endres mens en note holdes | Behold opprinnelig ut-note til dens tilhørende Note Off | En lang tone kan fortsette utenfor det nye settet. Ingen automatisk retuning. |
| Tomt akkordsett i Chord Notes | Undertrykk nye noter; behold allerede klingende bindinger | Pauser i kontrollakkorden gir ikke pass-through eller siste akkord. |
| Ukjent akkord i Chord Scale | Undertrykk nye noter; vis faktisk tonesett/ukjent navn | Ingen oppfunnet grunntone eller skjult skala. |
| Korreksjon slås av/på eller verts-bypass endres | Nye noter bruker den nye innstillingen; eksisterende bindinger beholdes | Note Off beregnes aldri på nytt. Verts-bypass stopper også genererte testnoter. |
| Samme innnote overlapper på samme kilde/kanal | Match Note Off med eldste ennå ikke avsluttede Note On (FIFO) | MIDI 1.0 uten note-ID kan ikke identifisere alle fysiske gjentakelser perfekt. |
| Ulike innnoter treffer samme ut-note på samme kanal | Første Note On starter ut-noten; siste tilhørende slipp avslutter den | Ingen retrigger ved kollisjon; den senere notens velocity lager ikke et nytt anslag. |
| Generert kilde stoppes | Slipp bare dens egne bindinger | En ekstern note som deler ut-tone skal fortsette så lenge den fortsatt er holdt. |
| Panic, transportstopp, seek/loop-sprang eller state-gjenoppretting | Slipp aktive Flysnapper-bindinger og nullstill historikk/akkordtilstand ved neste behandling | Gamle aktive noter gjenopplives ikke fra lagret state. Kontrollakkorden må leveres på nytt. |

De stille innnotene får også en avgrenset binding. En etterfølgende Note Off kan derfor ikke feilaktig avslutte en annen tone når akkorden senere blir gyldig.

## Løpsregelen

De to foregående tonene betyr her **innnotene før korreksjon**. Dette er arbeidstolkningen i handoveren. Ved entydig nærmeste kandidat brukes alltid denne. Ved lik avstand velges retningen fra de to foregående innnotene; to like foregående toner med aktuell note én halvtone under velger nedover, én halvtone over velger oppover. Ubrukelig historikk velger oppover.

Samtidige Note On og overlapp mellom ulike innnoter/kilder regnes som polyfon tvetydighet. Ingen framtidig note ventes på. Historikken nullstilles ved innstillingsendring, panic, kildestopp og tvetydighet. Denne enkle policyen krever ingen framtidige noter eller separate musikermoduler.

## Sustain, uttrykk og kontrollrolle

- Vanlig CC64 håndteres internt per kilde og kanal. Note Off utsettes til pedal opp. CC64 sendes ikke videre til mottakerinstrumentet, slik at kollisjoner og kildestopp kan avsluttes deterministisk.
- CC120/123 fra vanlig MIDI avslutter denne kildens noter på kanalen. CC121 frigjør internt sustain før kontrolleren sendes videre. Andre vanlige CC-er sendes videre som tilgjengelig fra wrapperen.
- CC66/69 (sostenuto/hold 2) videresendes i vanlig spilling. Global Panic, transportreset og overflow sender pedal opp på kanaler der slike pedaler ble videresendt ned, også om innnotenes bindinger allerede er avsluttet. Dette er sikker opprydding, ikke en ny musikalsk sustainregel. Kildespesifikk avslutning endrer ikke en annen kildes uttrykkspedaler; isolert kildeavslutning under slike eksterne holdpedaler er derfor ikke generell uttrykksstøtte.
- Hele kanal 16 er kontrollrolle. Kontrollnoter og andre meldinger på denne kanalen lekker ikke til mottakerinstrumentet. Kontroll-CC64 forlenger ikke akkordsettet. Kontroll-CC120/123 tømmer akkordsettet.
- MPE støttes ikke og må være deaktivert i verten. Pluginen kan ikke bevise alle vertsinnstillinger ut fra innkommende kanalnummer. Kanal 16 må ikke samtidig brukes som MPE-stemme.
- Kanalbasert pitch bend og pressure sendes som levert av adapteren, uten en vedtatt musikalsk bend-strategi. Bend kan derfor flytte faktisk lyd utenfor Allowed Notes. Dette er ingen påstand om full uttrykks-/MPE-kompatibilitet.
- VST3/JUCE er ikke en transparent rå MIDI-byteledning. Lange/systemeksklusive meldinger støttes ikke av Flysnapper og telles som avviste hendelser. Vertens støtte for CC/bend/pressure må testes separat fra motorens tester.

## Akkordtolkning og skalaoppsett

Numerisk gjenkjenning bevarer det korrigerte Buffkiller-biblioteket med 50 typer, tillatte utelatelser/tillegg, tilstedeværende grunntone og tillatte bassroller. Færre eksplisitte utelatelser prioriteres; bass på grunntonen rangeres foran ters og kvint. Ved gjenværende likhet brukes den stabile bibliotek-/kandidatordenen. Første kandidat styrer Chord Scale; alternative gyldige navn vises og skal ikke fremstilles som entydighet.

Skalatabellens fabrikkvalg er rimelige, redigerbare anbefalinger. Hver type har sitt eget valg i alle 12 grunntoner. Ingen familiearv kobler for eksempel m7, m9 og m13 sammen. Tilstand bruker stabile akkord- og skala-ID-er; nye/ukjente oppføringer skal ikke omskrive kjente valg. Flip gjenoppretter aldri standarder. `Reset all 50 to factory` er en uttrykkelig handling og endrer bare denne tabellen.

## Avgrensning og kapasitet

Kjernen har maksimalt 512 aktive innbindinger og 4096 ut-hendelser i en blokk. Wrapperen tar maksimalt 2048 innhendelser per blokk. Overflow er en feiltilstand med uttrykkelig opprydding; hendelser skal ikke samles i en voksende kø. Kjerne-overflow avslutter mottakernoter og krever panic/transportreset før nye toner sendes. Editoren viser `Capacity exceeded - press Panic` ved den låste kapasitetsfeilen og viser telleren for avviste/ikke støttede hendelser. Pitchmodulen har separat, eksplisitt grense på inntil seks samtidige detekterte noter.

Motoren og wrapperens private utbuffer er avgrenset og forhåndsallokert. Vertens egen MidiBuffer og JUCE/VST3-adapteren kan likevel måtte vokse. Derfor hevder vi ikke at hele den kompilerte VST3-kjeden er fri for heap-allokering. Motorens allokerings- og tidsmålinger må leses med sin faktiske målegrense; en rask funksjon eller et gjennomsnitt er ingen garanti for at alle blokker møter fristen.

Sidechain-status betyr aktivert audiobuss. Den sier ikke om Live har tilordnet en kilde, og digital stillhet brukes aldri som bevis på frakobling. Pitchmodulen analyserer audio og leverer ordinære noter på kanal 1. En skjult
automatiserbar rutingtest er beholdt for vertstester, med egen teknisk kilde-ID
slik at diagnostikk aldri avslutter detektorens eller keyboardets bindinger.

## Målinger

Pitchmodulens onset/release, velocity, mono/poly og bend-nærmeste-note-behandling
utvikles og testes på egne lyddata. Se `AUDIO_VALIDATION.md`, `TEST_RESULTS.md`
og installasjonspakkens `Validation/audio-validation` for faktiske resultater.
Ingen lavlatencypåstand følger bare av at en modul er implementert eller bygget.
