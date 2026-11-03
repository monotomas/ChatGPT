# Neste milepæl

0.1.0 er en avgrenset, testet rutings-/MIDI-prototype. Se TEST_RESULTS.md for
utført verifikasjon. Original handover med faseplan ligger under handover/.

1. Bygg på ekte Windows x64 via BUILD_FLYSNAPPER.bat eller den vedlagte manuelle
   GitHub Actions-arbeidsflyten. Den siste krever at kildeprosjektet først finnes
   i brukerens GitHub-repo. Ingenting er pushet her. Ikke kall Python-fixturetester
   for en gjennomført Windows-kompilering.
2. Utfør HOST_TEST_PLAN.md i Ableton Live 12 Suite. Løs eventuelle observerte
   bus-/kanal-/eventproblemer før større DSP-integrasjon. Noter særlig faktisk
   sidechain-status ved No Input; en aktiv buss alene beviser ikke kildetilkobling.
3. Avklar de få musikalske overgangene i PROTOTYPE_POLICIES.md med konkrete
   spillbare eksempler. Fabrikkanbefalingene for skalaer er allerede redigerbare
   og trenger ikke ny godkjenning rad for rad.
4. Etabler fysisk ruting-/mottakerforsinkelse med den genererte testnoten. Start
   deretter kausal mono-sammenligning og separat poly-gjennomførbarhet etter
   handoverens fase C/D. Bruk representative gitaropptak og tidsriktig fasit,
   ikke bare sinus eller kjøringstid for MIDI-korreksjonen. Falske første noter,
   manglende anslag, oktavfeil og release/retrigger må telle med i resultatene.

Nye detektorer skal levere vanlige tekniske Generated-hendelser til den samme
MIDI-korreksjonsmodulen. De skal ikke gå ut og inn via Live eller innføre en
uønsket tredje funksjonsmodul. Mono og poly er fortsatt produktmål; ingen av dem
er fjernet eller erklært oppfylt av denne prototypen.
