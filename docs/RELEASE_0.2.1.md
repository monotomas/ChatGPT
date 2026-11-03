# Flysnapper 0.2.1

Denne oppdateringen følger opp gitarens avklinging, klangendringer, falske
oktavskifter og manglende kontinuerlig bend i den første leveransen.

Detektoren skiller nå kravene til et nytt anslag fra videre følging av en
allerede identifisert tone. Periodisk toneinformasjon kan følges under terskelen
for nye noter. Gain-normalisert periodemåling tåler endret amplitude, mens
kontroller for faktisk demping, stillhet og støy hindrer at en ren holdetid
erstatter toneanalysen. Et etablert grunnlag beholdes gjennom visse endringer
i overtonenes innbyrdes styrke når den fysiske toneinformasjonen fortsatt finnes.

`Mono bend` legger til vanlig MIDI pitch wheel for bend og vibrato, med ett
fast noteanker gjennom toneforløpet. Velg Monophonic, slå på Mono bend og bruk
samme bendområde i mottakerinstrumentet: +/-2 eller +/-12 halvtoner. Flysnapper
sender standard RPN for området. Bend er valgfritt og starter avslått, også når
et eldre preset lastes. Poly bruker ikke en felles wheel for alle stemmene.

Audio med Mono bend bruker kanal 2; eksternt spille-MIDI beholdes på kanal 1
ved Flysnappers utgang. Mottaker/vert kan slå kanaler sammen eller bruke én
global wheel. Uavhengig uttrykk for begge kilder krever at MIDI-rutene og
mottakerne faktisk holder dem fra hverandre. MPE er ikke nødvendig for vanlig
monofonisk audio-bend.

De nye kontrollerte gitarprøvene målte falske oktaver **oppover** i 0.2.0.
Brukerens spesifikke spontane oktavhopp nedover ble ikke gjenskapt. Rettelsen
må derfor ikke omtales som dokumentert løsning på alle slike nedoverfeil.
Vanlig polyfonisk gjenkjenning har fortsatt kjente feil på krevende opptak.

Ansatsforsinkelsen er ikke vesentlig redusert. I de 107 tidligere kjente mono-
referansene er median/P95/maksimum fortsatt 45.979/74.229/96.604 ms. To opptak
starter 2.7 og 5.3 ms tidligere, ett roligere reanslag 2.7 ms senere. Resten har
samme tidspunkt. Dette er detektormålinger, uten lydkort, Ableton og instrument.
Målene på 5/10 ms fysisk kjede er fortsatt ikke oppfylt.

Detaljer og råmålinger ligger i `AUDIO_VALIDATION.md` og `validation/audio-update21`.
Kontrollene er regresjonsdata etter at de opprinnelige feilene ble målt.
Syntetiske bend-/klangprøver dokumenterer ikke vilkårlig gitarspill, og de
utvidede opptakshalene gjenbruker fire tidligere kjente gitaropptak.

Ved manuell oppdatering: lukk Ableton og erstatt hele den ytterste mappen
`Flysnapper.vst3`, med `Contents` og alt innhold, i den VST3-mappen du allerede
bruker. En ferdig Windows-installer følger også med. Kildebygging kreves ikke.
