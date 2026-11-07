# Feilrapport: retning og anslag

Brukeren bekreftet fungerende ruting i 0.4.2, men meldte at oppadgående/
nedadgående løp og tie-break ikke følges godt, og at nye anslag mister
rytmen når de korrigeres til samme tone som allerede klinger.

Bekreftede kodeårsaker: enhver forskjellig overlappende holdt input
nullstilte den globale historikken; outputreferansetellingen undertrykte
alle nye Note On når måltonen hadde en aktiv referanse. Begge var senere
implementasjonspolicyer, ikke låste krav i handoveren.

Den opprinnelige tie-tabellen beholdes. Eksempel 67,65 → aktuell 68 skal
fortsatt gi 67, fordi det er de to foregående råtonene som bestemmer
retningen. Legatooverlapp alene skal ikke slette dette grunnlaget.
Nye spilleanslag skal komme fram, også ved samme korrigerte tone, med
korrekte gamle og nye Note Off-bindinger og uten ekstra lookahead.

Routing, instrumentrolle, alle 52 akkordtyper/38 skalaer og fabrikkhash
`8372f68334a1ed37e09500c0b527151a73fa6c18988fd0a972bf933bc3fd779e`
beholdes. Brukerens tidligere GitHub-godkjenning dekker bygging og
publisering av prosjektfiler, Excel-grunnlag og byggelogger. Fulle private
Live-logger skal fortsatt ikke publiseres.

Tillegg fra brukeren: én unik akkordtone vises med `(single note)`, og
durtreklangen med `Major`. Uidentifiserte akkorder (også tomt input og én
tone) skal gi kromatisk spille-MIDI i begge akkordavhengige modusene.
Eksplisitt stoppet transport skal gi kromatisk spille-MIDI i alle tre
moduser, uten å skrive om lagrede parametere. Manglende transportinformasjon
skal ikke tolkes som stopp.

Tabellen skal sorteres for presentasjon uten å endre stabile typeindekser,
parameter-ID-er eller fabrikktilordninger. Brukerens siste presiseringer:
`C5` rett etter `Csus4`, `C6` etter `C5`, `C6/9` etter `C6` (tolkning av
en selvreferanse i brukerens tekst), `C5/7` rett etter `C7` som dominant,
og `Cm6` mellom `Cm` og `Cm7`. `mMaj`-akkordene inngår i mollgruppen.
Dette gir 15 dur/sus/power, 24 dominant, 12 moll og én dim7.

Kolonnelayout er fortsatt til diskusjon. Ved bevart rekkefølge, naturlig
tallsortering og alfabetisk modifikatorrekkefølge (`#`, `add`, `b`, `sus`)
gir 18/17/17 brudd mellom `C7#5` og `C7#5#9`, og mellom `C13#9` og
`C13add#9`. Hele hovedfamilier gir 15/24/13. Ingen tabellomlegging skal
starte før brukeren har fått denne tilbakemeldingen og valgt fordeling.

Senere presisering: `C5` flyttes til mollgruppen rett etter `Cm`, og
brukeren vurderer fire kolonner med 14/12/12/14 rader. Durgruppen blir
da 14; moll inklusive `C5` og `dim7` blir 14. De 24 dominanttypene deles
etter tolv: `C7b13` sist i kolonne 2, `C7sus4` først i kolonne 3 med
samme modifikatorsortering som ovenfor. Brukeren har bedt om å få vite
bruddet; selve firekolonnelayouten er ennå ikke bekreftet.

Brukeren har deretter uttrykkelig valgt fire kolonner med fordelingen
14/12/12/14 og dette bruddet. Implementasjon av oppsettet er godkjent.
