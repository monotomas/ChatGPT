# Vedlikehold og videre utvikling

Gjeldende bestilling er hele Flysnapper, se PRODUCT_SPEC.md. Eldre prototypestopp
og pålegg om at brukeren først må prøve ruting, er ikke gjeldende arbeidskrav.
Utvikleren eier feilretting, lydmålinger, byggekjede og installasjonspakking.

Før en ny Windows-pakke kan leveres skal alle obligatoriske tester kjøres på
faktisk Windows, og den uavhengige AudioQuality-kontrollen må bestå. Mål også
hele lydkorpuset og en separat kontrollklang med andre anslag/timbre. Ikke
endre annotasjoner eller godta feilnoter bare for å få grønt bygg.

Publisering av verifiserte installasjonspakker skjer automatisk fra
release/full-flysnapper til downloads/flysnapper-complete. Kildecommit,
Windows-/Unicode-bevis, testresultater og filhashes må passe sammen. Bruk
immutable GitHub-lenker i chatten, ikke interne workspace-/sandbox-lenker.

Brukeren skal ikke feilsøke kildekode, kompilere normale sluttbrukerpakker eller
bli delaktig i mellomliggende utviklerprøver. Eventuelle gjenstående begrensninger
må beskrives konkret i leveransen. Ikke erklær fysisk 5/10ms-latency eller faktisk
Ableton-godkjenning på grunnlag av motor- eller testvertresultater.
