# 0.4.1: MIDI-prosessor med stille instrumentutgang

Dette kontrollpunktet gjelder brukerens bestilte VST3-instrumenttilpasning,
etter [avslaget for 0.4.0](../live-open-failure-2026-11-07/README.md).
Kategorien er Instrument, med null audioinnganger og én aktiv stereo
audioutgang som alltid er stille. MIDI inn/ut og kanalrollene er beholdt.

Alle 52 fabrikkvalg fra den utfylte Excel-filen og alle 38 skalaer beholdes.
`Source/Core` er identisk med den endelige fabrikkutgaven 7323545.
Arbeidsboken har fortsatt SHA-256
`2df3afd9c7757469d8d7bf524efc3e2e09eb03824452ecbeb97294e3fafcc231`.

De nye kontrollene skal måle den kompilerte komponentens instrumentkategori,
audio-/eventbusser og stille stereoresultat under MIDI-behandling og bypass.
Eksisterende prøver av korreksjon, holdte noter, tilstand og fabrikkvalg
beholdes. Resultater legges til her etter kjøring; dette er ingen påstand om
at kontrollene allerede er bestått.

Ingen faktisk Ableton Live-installasjon er tilgjengelig i skymiljøet.
Vertsaksept og praktisk ruting av denne binærfilen gjenstår til brukerprøven.
CLAP, Max for Live og intern prosessorkjede er ikke implementert.
