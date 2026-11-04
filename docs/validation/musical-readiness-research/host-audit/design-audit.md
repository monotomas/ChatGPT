# Host and MPE design audit

Snapshot read 2026-11-04 00:54 UTC; files were actively being developed, so the findings below describe that snapshot, not necessarily the latest edit. This review did not edit or build project code.

Inspected `Source/PluginProcessor.{h,cpp}`, `Source/Pitch/MpeOutputRouter.{h,cpp}`, `Tests/MpeOutputRouterTests.cpp`, and pinned JUCE `ea77444750de45e036e0279bb0291144bf8e3688`. Manual provenance and exact quotes are in `report.md` alongside this file.

## Concrete mismatches

1. **Incoming MPE member pitch was decoded as legacy ±2.** The processor already forwards ordinary external channels 2-15 into the router, so simple note redistribution does not itself drop notes. But every input channel's cached range defaulted to 2, and incoming RPN 6 was ignored. A standard lower-zone setup only needs to announce its per-note range on the first member; this must update the entire input zone. Counterexample: RPN6 master1=14, RPN0 member2=48, NoteOn member3=60, wheel member3=9216 should produce pitch 66; the reviewed implementation produces 60.25 because member3 retains range 2.

2. **Incoming master controls did not reach incoming members.** All input controller routing matched only `slot.inputChannel == incomingChannel`. Once Live allocates keyboard notes to member channels, master1 sustain, pitch, pressure and timbre must fan to the re-encoded external voices on those members. Master pitch adds to member pitch; it must never be forwarded to the outgoing master, where it would bend generated audio voices. Sustain down on master1 followed by NoteOn/Off member3 was an explicit failing semantic sequence in the reviewed design.

3. **Outgoing master range was zeroed without centering the master wheel.** `ensureZone()` sent RPN0 range0 but no center wheel. A prior legacy channel1 wheel value can remain cached in a host/receiver, and a receiver which fixes or ignores master bend-range RPN would then bias every outgoing member. A setup-only master1 center message before any new MPE voice is appropriate. Tests should forbid noncenter master expression, not forbid the harmless initial center message.

4. **A new receiver could miss zone configuration after the first phrase.** `zoneConfigured` stayed true after all voices were released. `prepareSlot()` re-sent member range before every note but not MCM/master range. A fresh native JUCE MPEInstrument inserted after the original phrase would receive notes on channels outside any active zone and ignore them. Re-announcing zone/ranges on a transition from no allocated voices to the first new voice avoids this case without changing configuration under held notes. Live's own receiver configuration may independently cover this, but the router's general receiver guarantee should be accurate.

5. **MCM used an unnecessary 14-bit Data Entry LSB.** The generic `rpn()` helper emits CC6 followed by CC38 for RPN6 too. JUCE's canonical MPEMessages::setLowerZone uses a 7-bit RPN6 message (CC6 only). JUCE's parser applies the correct first CC6 value and then ignores the combined 14-bit value >15, so the reviewed tests still pass. Sending the canonical 7-bit MCM removes that ambiguity in other hosts/receivers. This is a robustness improvement, not evidence of an actual observed Live failure.

## Feasible mitigation within the current architecture

Root selected an explicit mixed-input contract for MPE Output mode: ordinary playing channel1 defaults ±2, lower-zone input members2-15 default ±48, chord-control channel16 remains separate. This is appropriate for the mandatory source-device Enable MPE configuration and preserves the user's documented channel1/channel16 workflow. Incoming RPN6/RPN0 should update/deactivate this zone, and master state should combine with member state for external re-encoded voices. Legacy output mode should retain its ordinary independent-channel behavior.

The MPE implementation owner has been notified and is implementing the mixed-input contract and setup-only master center. The late-receiver and MCM points were also sent for review.

## Host facts versus remaining runtime evidence

- The pinned Live 12 manual explicitly says MIDI-output plug-ins with MPE enabled can output MPE. This is strong evidence of feasibility.
- Its MPE/Multi-channel Settings configure channels Live sends **to** plug-ins or external devices. The text does not guarantee that setting an input lower zone also configures outgoing plug-in decoding.
- The manual documents direct internal track-to-plug-in target MIDI-channel routing, but does not expressly say what happens to a targeted channel16 chord route when source-device MPE is enabled.
- A default lower zone of 15 members could let Live allocate a playing note on16, where Flysnapper would interpret it as chord control. Lower14 input configuration is therefore necessary for this contract; whether Live preserves the explicit chord16 route outside that zone remains unverified.
- JUCE preserves the incoming VST3 channel fields and IMidiMapping controller-channel assignment. It does not recover a source-track identity after a host remaps channels. If Live redistributes targeted chord notes into playing members, no algorithm inside the present single MIDI input bus can reliably infer which notes were chords. Do not claim automatic reconstruction of discarded provenance.
- JUCE VST3 emits channelized note and LegacyMIDICCOut pitch-wheel output. Its ordinary MidiBuffer path ignores VST3 Note Expression value/text objects, and `supportsMPE()` is not consulted in this wrapper. Returning true alone neither switches Live's device MPE mode nor proves host integration.
- Native JUCE MPE receiver tests verify real per-note pitch semantics and are useful. They do not verify Live's plug-in-output decoder, direct chord16 routing under MPE mode, or routing per-note pitch into the actual receiving instrument.

No user question, beta installer, virtual MIDI port, or custom Note Expression architecture is justified by this audit. The unresolved item is actual host integration evidence, not a documented lack of Live capability.
