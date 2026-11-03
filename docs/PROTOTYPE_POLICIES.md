# Flysnapper 0.1.0 – uttrykkelige testregler

Denne versjonen er en rutings-/MIDI-prototype. Reglene nedenfor gjør den testbar uten å late som handoverens åpne musikalske spørsmål er avgjort. De er **foreløpige implementeringsvalg**, ikke godkjente sluttproduktstandarder. Ingen av dem dokumenterer pitch-to-MIDI eller oppnådd live latency.

## Fastlagte regler som er implementert

- Tre moduser i MIDI-to-MIDI: Chord Notes, Chord Scale og Fixed Scale. Korreksjon kan omgås.
- Korreksjon finner nærmeste tillatte MIDI-note innen 0–127, på tvers av oktavgrenser. Mottatte akkorders register begrenser ikke utøverens register.
- Chord Notes bruker bare de faktisk holdte pitch classes. Duplikater og oktavdoblinger teller hver for seg ved Note Off.
- Chord Scale bruker gjenkjent type og grunntone til å slå opp brukerens valgte skala. Den forener ikke skalaen med akkordtonene i skjul. Alle 50 typer har egne, stabile ID-er og alle 19 skalavalg.
- Fixed Scale bruker manuelt valgt grunntone og skala. Chromatic tillater alle pitch classes.
- Note Off følger bindingen opprettet av Note On, selv når akkord eller oppsett er endret. Velocity-zero Note On er Note Off. Vanlig ekstern notevelocity beholdes ved en ny ut-note.
- Samtidige kontrollhendelser på samme sample behandles først; deretter dannes nye utøvernoter mot dette settet. Hendelser på senere sample i samme blokk påvirker ikke tidligere noter.
- Intern testnote og eksternt MIDI bruker samme korreksjon. De er separate tekniske kilder, ikke egne musikermoduler. Ingen ekstra blokk eller framtidig note ventes på.
- GUI, flip og lukket editor styrer ikke notenes tid eller levetid. Allowed Notes kommer fra det aktive numeriske prosesseringssettet.

## Foreløpige overganger og noteidentitet

| Situasjon | Prototypens uttrykkelige regel | Musikalsk konsekvens |
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

Samtidige Note On og overlapp mellom ulike innnoter/kilder regnes som polyfon tvetydighet. Ingen framtidig note ventes på. Historikken nullstilles ved innstillingsendring, panic, kildestopp og tvetydighet. Denne enkle policyen skal vurderes med faktisk legato/retrigger-spilling før den fryses som produktadferd.

## Sustain, uttrykk og kontrollrolle

- Vanlig CC64 håndteres internt per kilde og kanal. Note Off utsettes til pedal opp. CC64 sendes ikke videre til mottakerinstrumentet, slik at kollisjoner og kildestopp kan avsluttes deterministisk.
- CC120/123 fra vanlig MIDI avslutter denne kildens noter på kanalen. CC121 frigjør internt sustain før kontrolleren sendes videre. Andre vanlige CC-er sendes videre som tilgjengelig fra wrapperen.
- CC66/69 (sostenuto/hold 2) videresendes i vanlig spilling. Global Panic, transportreset og overflow sender pedal opp på kanaler der slike pedaler ble videresendt ned, også om innnotenes bindinger allerede er avsluttet. Dette er sikker opprydding, ikke en ny musikalsk sustainregel. Kildespesifikk avslutning endrer ikke en annen kildes uttrykkspedaler; isolert kildeavslutning under slike eksterne holdpedaler er derfor ikke generell uttrykksstøtte.
- Hele kanal 16 er kontrollrolle. Kontrollnoter og andre meldinger på denne kanalen lekker ikke til mottakerinstrumentet. Kontroll-CC64 forlenger ikke akkordsettet. Kontroll-CC120/123 tømmer akkordsettet.
- MPE støttes ikke og må være deaktivert i verten. Pluginen kan ikke bevise alle vertsinnstillinger ut fra innkommende kanalnummer. Kanal 16 må ikke samtidig brukes som MPE-stemme.
- Kanalbasert pitch bend og pressure sendes som levert av adapteren, uten en vedtatt musikalsk bend-strategi. Bend kan derfor flytte faktisk lyd utenfor Allowed Notes. Dette er ingen påstand om full uttrykks-/MPE-kompatibilitet.
- VST3/JUCE er ikke en transparent rå MIDI-byteledning. Lange/systemeksklusive meldinger støttes ikke av denne prototypen og telles som avviste hendelser. Vertens støtte for CC/bend/pressure må testes separat fra motorens tester.

## Akkordtolkning og skalaoppsett

Numerisk gjenkjenning bevarer det korrigerte Buffkiller-biblioteket med 50 typer, tillatte utelatelser/tillegg, tilstedeværende grunntone og tillatte bassroller. Færre eksplisitte utelatelser prioriteres; bass på grunntonen rangeres foran ters og kvint. Ved gjenværende likhet brukes den stabile bibliotek-/kandidatordenen. Første kandidat styrer Chord Scale; alternative gyldige navn vises og skal ikke fremstilles som entydighet.

Skalatabellens fabrikkvalg er rimelige, redigerbare anbefalinger. Hver type har sitt eget valg i alle 12 grunntoner. Ingen familiearv kobler for eksempel m7, m9 og m13 sammen. Tilstand bruker stabile akkord- og skala-ID-er; nye/ukjente oppføringer skal ikke omskrive kjente valg. Flip gjenoppretter aldri standarder. `Reset all 50 to factory` er en uttrykkelig handling og endrer bare denne tabellen.

## Avgrensning og kapasitet

Kjernen har maksimalt 512 aktive innbindinger og 4096 ut-hendelser i en blokk. Wrapperen tar maksimalt 2048 innhendelser per blokk. Overflow er en feiltilstand med uttrykkelig opprydding; hendelser skal ikke samles i en voksende kø. Kjerne-overflow avslutter mottakernoter og krever panic/transportreset før nye toner sendes. Editoren viser `Capacity exceeded - press Panic` ved den låste kapasitetsfeilen og viser telleren for avviste/ikke støttede hendelser. Dette skal ikke forveksles med en musikalsk polyfonibegrensning for den ennå ikke implementerte pitchdetektoren.

Motoren og wrapperens private utbuffer er avgrenset og forhåndsallokert. Vertens egen MidiBuffer og JUCE/VST3-adapteren kan likevel måtte vokse. Derfor hevder vi ikke at hele den kompilerte VST3-kjeden er fri for heap-allokering. Motorens allokerings- og tidsmålinger må leses med sin faktiske målegrense; en rask funksjon eller et gjennomsnitt er ingen garanti for at alle blokker møter fristen.

Sidechain-status betyr aktivert audiobuss. Den sier ikke om Live har tilordnet en kilde, og digital stillhet brukes aldri som bevis på frakobling. Den eneste genererte lydrelaterte MIDI-kilden i denne versjonen er den navngitte rutingtesten: MIDI 60, velocity 100, 100 ms. Den må ikke tolkes som resultat av tonegjenkjenning.

## Få valg før neste musikalske milepæl

Etter den avgrensede Live-prøven trengs konkrete valg om:

1. **Holdte noter og tom kontrollakkord:** Skal en holdt C fortsette når akkorden skifter til D, og skal et opphold i akkordsporet stoppe nye noter, la dem gå gjennom eller beholde siste sett?
2. **Sustain og kollisjoner:** Skal to innnoter som korrigeres til samme C gi ett langt C, et nytt anslag, eller annen avtalt slippadferd? Skal kontrollpedalen holde akkordsettet?
3. **Historikk og flertydighet:** Skal melodiretning bygge på inn- eller uttoner, og er første viste kandidat riktig styring for eksempelvis flertydige inversjoner i Chord Scale?

Detektorens onset/release, velocity, legato/retrigger og bend-regler må deretter velges fra målinger på representative gitaropptak. Mono og poly er fortsatt produktkrav. Ingen målt grense eller brukerbeslutning er erstattet av prototypens testregler.
