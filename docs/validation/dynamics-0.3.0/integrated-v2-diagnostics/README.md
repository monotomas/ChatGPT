# Integrated post-conditioning diagnostic, before the plain-G1 fix

This exact frozen DSP edition places dynamics AFTER the original internal
stationary-noise conditioning at the detector analysis rate. The driver passes
raw input to the actual detector and configures its integrated dynamics; no
external waveform conditioning approximation is used. The independent capture
reproduces the accepted prototype's counts, but native3 still fails a separate
plain-mono quiet-G1 guard. Consequently this is a historical diagnostic, not
approval of a final installer.

| Suite | Previous frozen source, dynamics off | Integrated default dynamics |
| --- | ---: | ---: |
| Controlled scoped configurations |35/65|42/65|
| Real isolated hammers |8/16|8/16|
| Recorded-noise controls |45/128|48/128|
| Total scoped |88/209|98/209|

All263 expected reference notes remain counted:243 matched previously,245 now.
Extras fall393→324, premature first offs54→51, but late offs increase6→9.
Controlled noise-only ons increase26→32, while recorded noise-only ons fall
72→42. The gains do not establish general noise rejection. Every per-case
failure, miss, wrong note, release error and raw event remains available.
Seven historical continuous-poly-bend cases are raw captured but listed
outside the explicitly revised user scope, never turned into passes.

All broad mono fixtures enable monophonic bending. Their quiet-mains passes
cannot substitute for plain mono coverage: the actual-processor native3 guard
finds G1→G2 aliases in plain mode, even with dynamics bypassed. See the separate
native3 and bare-baseline archives. A later fix needs fresh exact-source proof.

The independent real hammer samples have known early acoustic-pitch/nominal-SFZ
anchor uncertainty; their original labels and failures are unchanged. No new
complete continuous-human-technique coverage or physical audio-interface
measurement is asserted. Latency tables explicitly retain misses and are
conditional on matched references.

Both frozen source editions, exact driver/scorer/policy bytes, raw captures,
compiler commands, hashes and nonzero score exits are retained. All compiled
source/driver/scorer bytes and output inventories were verified during archive
creation. Original PCM remains in its sealed repository directories; no binary
or audio duplicate is included. Historical absolute/tmp paths in manifests
identify the original run locations and have not been rewritten.
