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
publisering av prosjektfiler, Excel-grunnlag og byggelogger. Full private
Live-logger skal fortsatt ikke publiseres.
