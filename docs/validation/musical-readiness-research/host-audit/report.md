# Flysnapper: Ableton Live MPE output feasibility

Research date: 2026-11-04. Read-only research; no Live installation or execution was available. No project edits were made.

## Evidence and provenance

A public repository contains a real text copy of the Live 12 manual, not a forwarding proxy:

- [Pinned manual copy](https://github.com/J0rgeSerran0/Ableton_Live_Manual_to_HTML_and_PDF/blob/4bb26b7814a581cf6d460ff4e5f04036daef23fe/docs/Ableton_Live_12-en.md)
- Repository README dates generation to 2024-05-24 12:00AM.
- Local text: `/tmp/flysnapper-mpe-host/Ableton_Live_12-en.md`.
- Bytes: 1,477,912. SHA256: `5ed4aea3a52d126093ac0ecc32a4e3061437399cadf8e55525180918769ce10c`.
- Original official chapter link, **not fetched** because official Ableton hosts were outside the environment's network allowlist: <https://www.ableton.com/en/live-manual/12/editing-mpe/#mpe-in-external-plug-ins>.

This is a pinned, historical third-party copy of the manual, not a claim that the current official page was inspected. Its generated image links are unreliable placeholders; conclusions below come from text only.

## What the manual does establish

**High confidence: Live supports MPE output from a MIDI-output plug-in, when MPE is enabled for that device.** Section 10.5, local line 4466, explicitly states:

> Plug-ins that have MIDI outs and that have MPE enabled can also output MPE.

The preceding text (4460-4463) says:

> MPE data for MPE-enabled plug-ins can also be accessed and modulated in Live.
>
> The enabled/disabled state of a plug-in device’s MPE Mode will be saved with that device’s default configuration.

This is decisive evidence that an in-process VST MIDI-output MPE approach is viable in Live. There is no justification for claiming that a virtual MIDI port or custom Note Expression wrapper is categorically required.

**High confidence: Live can explicitly configure a lower MPE zone with a selected number of note channels.** Section 10.6.1, local line 4510:

> For MPE-enabled plug-ins, you can find these settings in the right-click (PC) / CTRL-click (Mac) context menu of the respective device’s title bar.

Section 10.6.2, local lines 4532-4543, says the settings:

> configure the MPE zone and range of note channels used by Live when sending MPE to an external MIDI device or plug-in
>
> select the upper or lower zone and number of note channels
>
> select multi-channel mode, which sets an arbitrary range of note channels
>
> Each zone needs a global channel (for non-polyphonic controls). The global channel for the lower zone is Channel 1, and Channel 16 is for the upper zone.

Lower zone with 14 members means master 1, members 2-15; it deliberately excludes channel 16. Do not use a default lower zone of 15 members if channel 16 remains reserved for chord-control input.

**Important qualification:** the manual describes these zone settings as configuring data Live sends **to** a plug-in/device. It does not explicitly say that those settings configure how Live decodes that plug-in's outgoing MIDI, nor that this prevents Live remapping that plug-in's incoming ordinary MIDI. Document source device MPE enable and receiver MPE configuration separately.

**High confidence: Live supports targeting a plug-in input MIDI channel through internal track routing in general.** Section 16.5.2.6, local line 6387:

> Each additional MIDI track has its Output Type chooser pointed to the track that contains the instrument, and its Output Channel chooser referencing the target MIDI channel.

This supports the user's existing chord track direct routing to channel 16, which the user already confirmed worked in the prior release. It does not expressly prove the same routing remains unaltered after enabling MPE on the target device.

**High confidence: hardware/controller input MPE changes channel-selection UI.** Section 10.1, local line 4315:

> Note that when selecting a MIDI controller that has MPE enabled as an input device on a track, the channel input routing is fixed to “All Channels’’ and no individual channels can be selected.

This statement is about an MPE-enabled **MIDI controller selected as track input**. It cannot be cited as proof that a VST plug-in's direct targeted input channels are remapped or preserved.

## JUCE wrapper evidence

Inspected local JUCE checkout: `ea77444750de45e036e0279bb0291144bf8e3688`.

Public pinned source: <https://github.com/juce-framework/JUCE/blob/ea77444750de45e036e0279bb0291144bf8e3688/modules/juce_audio_processors_headless/format_types/juce_VST3Common.h>.

- Lines 1292-1293 translate JUCE channels 1-16 to VST3 channels 0-15 and back without allocating MPE channels.
- Lines 1301-1325 encode note channels into VST3 note events. Note IDs are -1, not a plugin-managed VST3 per-note identifier scheme.
- Lines 1340-1346 encode `LegacyMIDICCOutEvent.channel` from the original MIDI channel.
- Lines 1382-1388 encode pitch wheel as `LegacyMIDICCOutEvent` with `kPitchBend` and the original wheel bytes.
- Lines 1512-1532 reconstruct incoming JUCE MIDI note channels from the VST3 event channels; the wrapper itself does not collapse channel 16 to 1.
- Lines 1534-1538 ignore VST3 Note Expression value/text events in the ordinary MIDI conversion path.
- The VST3 processor wrapper consumes this conversion at `juce_audio_plugin_client_VST3.cpp:3591`.
- `supportsMPE()` is consulted in AU/VST2 wrappers, but no invocation was found in the VST3 wrapper. Overriding it cannot by itself be treated as proof Live enables MPE for this VST3.

These facts prove the native wrapper can carry channelized MIDI note/pitch-wheel output. They do not prove Live's subsequent decoding or channel routing, and do not prove what channels Live allocates before reaching that wrapper when source device MPE is enabled.

## Recommended implementation and limits of proof

Use ordinary MIDI MPE, one member channel per simultaneous sounding note, with the agreed lower zone master 1/member 2-15 and ±48 semitone member range. Keep channel 16 out of output. Do not put normal external keyboard notes on the lower-zone master channel when generated voices coexist: master pitch bend applies globally. Encode those external notes on separate members and fan external input wheel only to those external members. The agreed generated members 2-7 / external members 8-15 is a workable internal allocation; Live may reallocate members while preserving per-note semantics, so do not require those exact member numbers downstream.

A native JUCE MPEInstrument receiver harness can prove correct zone setup, independent pitch, member reuse/reset, no master note-on, no chord-channel output, and coexistence of generated and external notes. A VST3 event conversion harness can separately prove those events survive the JUCE output wrapper. Neither harness is an Ableton Live integration test.

Remaining **unverified host integration points**:

1. Enabling MPE on the Flysnapper source device actually decodes outgoing channelized wheel into independent Live per-note pitch in the supported Live version.
2. Exact keyboard/chord input channel behavior after enabling MPE on Flysnapper. The existing engine assumption external playing channel 1 + chord-control channel 16 must not silently break if Live allocates incoming notes to members.
3. Routing the source plug-in's MPE output through the existing MIDI From/device endpoint to an MPE-capable receiver preserves independent pitch. A receiver also needs MPE enabled and a compatible pitch range; receiving all member wheels as ordinary global pitch would be invalid.

The historical manual supports feasibility strongly. These runtime checks remain necessary before claiming Ableton-specific completion; do not substitute a user beta test or label a native harness as that proof. A custom JUCE VST3 Note Expression extension is unnecessary at this stage and has no demonstrated Live output requirement in this evidence.
