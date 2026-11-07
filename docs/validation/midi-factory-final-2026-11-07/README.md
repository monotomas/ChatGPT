# Kontrollert lokalt: fire fabrikkrettelser i0.4.2

Alle52 akkordtyper og38 skalaer beholdes. C7#9 bruker Half Whole
Diminished13, C7b13 bruker Mixolydianb6 21, C13#9 og C13add#9
bruker Chromatic0. Gjenkjenning, ID-er, formler, MIDI-regler og vanlig
UI-kommentering er uendret. Alle52 fabrikkvalg inneholder nå hele
akkordformelen.

Lokale native4/4 grupper består: Core2191398 kontroller, GUI20315,
prosessor120459/15grupper/2387hendelser, faktisk VST3-rapport5642/23tilfeller/
1128hendelser; stdout5643 inkluderer siste rapportskrivekontroll.
Python199 totalt:197 bestått,2 forventede Windows-BAT-hopp påLinux.

21 utvalgte prosjektinnganger er identiske før/etter kjøring og stemmer
med rårapporter/modulbinding. Manifestets source_commit er null fordi
koden ble bygget før commit; filhashene identifiserer de faktiske valgte
inndataene, ikke full kompilator-/systemheader-/JUCE-lukning. Modulhash:
44ddc3f8ef92a78839bc0482688d55dc47b8c80bbe6aa22b04b91cbbbb434d58.

Kanonisk mappinghash:
8372f68334a1ed37e09500c0b527151a73fa6c18988fd0a972bf933bc3fd779e.
Friske instanser prøver alle fire nye verdier med faktiske MIDI-noter.
Skjermbildene er visuelt kontrollert; C13add#9 viser Factory:Chromatic,
vanlig grønn tonetekst og ingen særforklaring. Begge C13-rader er også
kontrollert direkte av GUI-testen.

Ingen ny Windows0.4.2-bygging/installasjon eller faktisk Ableton-kjøring
er utført. Tidligere avvisning av bred ekstern publisering skal respekteres;
ny GitHub-bygging/publisering venter på avklart tillatelse. Avbrutt51-
bibliotek og fargeunntak er lokale historiske forsøk, ikke aktive krav.
